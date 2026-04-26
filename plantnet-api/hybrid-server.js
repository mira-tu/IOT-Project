// EcoDex Hybrid API Server
// Uses LOCAL MODEL first (teammate's work), then Pl@ntNet API as fallback

require('dotenv').config();

const express = require('express');
const multer = require('multer');
const axios = require('axios');
const FormData = require('form-data');
const cors = require('cors');
const fs = require('fs');

const app = express();
const upload = multer({ dest: 'uploads/' });

// Configuration
const PLANTNET_API_KEY = process.env.PLANTNET_API_KEY || 'YOUR_API_KEY_HERE';
const LOCAL_MODEL_URL = process.env.LOCAL_MODEL_URL || 'http://127.0.0.1:8090';
const PORT = process.env.PORT || 3000;
const CONFIDENCE_THRESHOLD = parseFloat(process.env.CONFIDENCE_THRESHOLD || '0.65');

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

// Health check
app.get('/health', (req, res) => {
  res.json({ 
    status: 'ok', 
    service: 'EcoDex Hybrid API',
    localModel: LOCAL_MODEL_URL,
    plantnetAPI: 'enabled'
  });
});

// Check if local model is available
async function checkLocalModel() {
  try {
    const response = await axios.get(`${LOCAL_MODEL_URL}/health`, { timeout: 2000 });
    return response.status === 200;
  } catch (error) {
    return false;
  }
}

// Try local model first
async function tryLocalModel(imageBuffer) {
  try {
    console.log('🔬 Trying local trained model...');
    const response = await axios.post(
      `${LOCAL_MODEL_URL}/predict`,
      imageBuffer,
      {
        headers: { 'Content-Type': 'application/octet-stream' },
        timeout: 5000
      }
    );

    const result = response.data;
    console.log(`   Local model: ${result.label} (${(result.confidence * 100).toFixed(1)}% confidence)`);

    // Return local result if confident and not unknown
    if (result.accepted && result.label !== 'unknown' && result.confidence >= CONFIDENCE_THRESHOLD) {
      return {
        source: 'local_model',
        success: true,
        confidence: result.confidence * 100,
        plant: {
          scientificName: result.facts.scientific_name || result.label,
          commonName: result.display_name,
          family: 'See plant facts',
          about: result.facts.about,
          light: result.facts.light,
          water: result.facts.water,
          funFact: result.facts.fun_fact
        },
        raw: result
      };
    }

    // Low confidence or unknown - will try API
    return {
      source: 'local_model',
      success: false,
      confidence: result.confidence * 100,
      reason: result.label === 'unknown' ? 'Plant not in trained dataset' : 'Low confidence',
      raw: result
    };

  } catch (error) {
    console.log('   ⚠️  Local model unavailable:', error.message);
    return {
      source: 'local_model',
      success: false,
      reason: 'Local model offline',
      error: error.message
    };
  }
}

// Fallback to Pl@ntNet API
async function tryPlantNetAPI(imagePath, organ = 'leaf') {
  try {
    console.log('🌐 Falling back to Pl@ntNet API...');
    
    const form = new FormData();
    form.append('images', fs.createReadStream(imagePath));
    form.append('organs', organ);

    const response = await axios.post(
      `https://my-api.plantnet.org/v2/identify/useful?api-key=${PLANTNET_API_KEY}&include-related-images=true`,
      form,
      {
        headers: form.getHeaders(),
        timeout: 15000
      }
    );

    const plantnetData = response.data;
    
    if (!plantnetData.results || plantnetData.results.length === 0) {
      return {
        source: 'plantnet_api',
        success: false,
        reason: 'No results from Pl@ntNet'
      };
    }

    const topResult = plantnetData.results[0];
    const confidence = topResult.score * 100;

    console.log(`   Pl@ntNet: ${topResult.species.scientificNameWithoutAuthor} (${confidence.toFixed(1)}% confidence)`);

    return {
      source: 'plantnet_api',
      success: true,
      confidence: confidence,
      plant: {
        scientificName: topResult.species.scientificNameWithoutAuthor,
        commonName: topResult.species.commonNames?.[0] || 'No common name',
        family: topResult.species.family.scientificNameWithoutAuthor,
        genus: topResult.species.genus.scientificNameWithoutAuthor,
        images: topResult.images?.slice(0, 3).map(img => img.url.o) || [],
        gbifId: topResult.gbif?.id || null
      },
      allResults: plantnetData.results.slice(0, 3).map((r, i) => ({
        rank: i + 1,
        scientificName: r.species.scientificNameWithoutAuthor,
        commonNames: r.species.commonNames || [],
        confidence: Math.round(r.score * 100)
      })),
      remainingIdentifications: plantnetData.remainingIdentificationRequests
    };

  } catch (error) {
    console.error('   ❌ Pl@ntNet API error:', error.response?.data?.message || error.message);
    return {
      source: 'plantnet_api',
      success: false,
      reason: 'Pl@ntNet API failed',
      error: error.response?.data?.message || error.message
    };
  }
}

// Main hybrid identification endpoint
app.post('/api/identify', upload.single('image'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No image file provided' });
    }

    const organ = req.body.organ || 'leaf';
    const imageBuffer = fs.readFileSync(req.file.path);

    console.log('\n🌿 === HYBRID IDENTIFICATION START ===');

    // Step 1: Try local trained model
    const localResult = await tryLocalModel(imageBuffer);

    if (localResult.success) {
      // Local model succeeded!
      console.log('✅ Using LOCAL MODEL result (teammate\'s trained model)');
      fs.unlinkSync(req.file.path); // Clean up
      
      return res.json({
        success: true,
        source: 'local_model',
        message: `Identified using your trained model: ${localResult.plant.commonName}`,
        confidence: localResult.confidence,
        plant: localResult.plant,
        trainedPlants: ['aloe_vera', 'pinstripe_calathea', 'snake_plant', 'spider_plant']
      });
    }

    // Step 2: Local model failed or low confidence - try Pl@ntNet
    console.log(`⚠️  Local model: ${localResult.reason}`);
    console.log('🔄 Trying Pl@ntNet API as fallback...');

    const apiResult = await tryPlantNetAPI(req.file.path, organ);
    fs.unlinkSync(req.file.path); // Clean up

    if (apiResult.success) {
      console.log('✅ Using PL@NTNET API result (77,000+ species database)');
      
      return res.json({
        success: true,
        source: 'plantnet_api',
        message: `Identified using Pl@ntNet API: ${apiResult.plant.scientificName}`,
        confidence: apiResult.confidence,
        plant: apiResult.plant,
        allResults: apiResult.allResults,
        localModelAttempt: {
          tried: true,
          reason: localResult.reason,
          confidence: localResult.confidence
        }
      });
    }

    // Both failed
    console.log('❌ Both local model and Pl@ntNet API failed');
    
    return res.status(500).json({
      success: false,
      message: 'Could not identify plant with either method',
      localModel: localResult,
      plantnetAPI: apiResult
    });

  } catch (error) {
    if (req.file) fs.unlinkSync(req.file.path);
    console.error('Error:', error);
    res.status(500).json({
      error: 'Identification failed',
      message: error.message
    });
  }
});

// Raw image endpoint (for ESP32-CAM)
app.post('/api/identify-raw', async (req, res) => {
  try {
    if (!req.body || req.body.length === 0) {
      return res.status(400).json({ error: 'No image data provided' });
    }

    const organ = req.query.organ || 'leaf';
    const tempPath = `uploads/esp32-${Date.now()}.jpg`;
    fs.writeFileSync(tempPath, req.body);

    console.log('\n🌿 === HYBRID IDENTIFICATION (ESP32) ===');

    // Step 1: Try local model
    const imageBuffer = fs.readFileSync(tempPath);
    const localResult = await tryLocalModel(imageBuffer);

    if (localResult.success) {
      console.log('✅ Using LOCAL MODEL result');
      fs.unlinkSync(tempPath);
      
      return res.json({
        success: true,
        source: 'local_model',
        plant: localResult.plant.commonName,
        scientificName: localResult.plant.scientificName,
        commonName: localResult.plant.commonName,
        family: localResult.plant.family,
        confidence: localResult.confidence,
        light: localResult.plant.light,
        water: localResult.plant.water,
        about: localResult.plant.about
      });
    }

    // Step 2: Try Pl@ntNet API
    console.log(`⚠️  Local: ${localResult.reason}, trying API...`);
    const apiResult = await tryPlantNetAPI(tempPath, organ);
    fs.unlinkSync(tempPath);

    if (apiResult.success) {
      console.log('✅ Using PL@NTNET API result');
      
      return res.json({
        success: true,
        source: 'plantnet_api',
        plant: apiResult.plant.scientificName,
        scientificName: apiResult.plant.scientificName,
        commonName: apiResult.plant.commonName,
        family: apiResult.plant.family,
        confidence: apiResult.confidence
      });
    }

    // Both failed
    return res.status(500).json({
      success: false,
      error: 'Could not identify plant'
    });

  } catch (error) {
    console.error('Error:', error);
    res.status(500).json({
      error: 'Identification failed',
      message: error.message
    });
  }
});

// Get info about trained plants
app.get('/api/trained-plants', (req, res) => {
  res.json({
    count: 4,
    plants: [
      { name: 'Aloe Vera', photos: 40, trained: true },
      { name: 'Pinstripe Calathea', photos: 82, trained: true },
      { name: 'Snake Plant', photos: 80, trained: true },
      { name: 'Spider Plant', photos: 21, trained: true }
    ],
    fallback: 'Pl@ntNet API (77,000+ species)',
    localModelUrl: LOCAL_MODEL_URL
  });
});

// Start server
app.listen(PORT, async () => {
  console.log(`\n🌿 EcoDex HYBRID API Server running on port ${PORT}`);
  console.log(`📡 Endpoints:`);
  console.log(`   POST /api/identify - Upload image for identification`);
  console.log(`   POST /api/identify-raw - Identify from raw data (ESP32)`);
  console.log(`   GET  /api/trained-plants - List trained plants`);
  console.log(`\n🔬 Local Model: ${LOCAL_MODEL_URL}`);
  
  const localAvailable = await checkLocalModel();
  if (localAvailable) {
    console.log(`   ✅ Local model is ONLINE (teammate's trained model)`);
    console.log(`   📚 Trained plants: Aloe Vera, Pinstripe Calathea, Snake Plant, Spider Plant`);
  } else {
    console.log(`   ⚠️  Local model is OFFLINE - will use Pl@ntNet API only`);
    console.log(`   💡 Start local model: cd plant_cam_v4/ecodex_local && python server.py`);
  }
  
  console.log(`\n🌐 Pl@ntNet API: ${PLANTNET_API_KEY ? 'Configured' : 'NOT CONFIGURED'}`);
  console.log(`   📊 Fallback for unknown plants (77,000+ species)`);
  console.log(`\n🎯 Strategy: Local model first, Pl@ntNet API as fallback`);
  console.log(`   Confidence threshold: ${(CONFIDENCE_THRESHOLD * 100).toFixed(0)}%\n`);
});

module.exports = app;
