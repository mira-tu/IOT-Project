// EcoDex Backend API Server with Pl@ntNet Integration
// Install: npm install express multer axios form-data cors dotenv

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
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public')); // Serve static files from public folder

// Health check endpoint
app.get('/health', (req, res) => {
  res.json({ status: 'ok', service: 'EcoDex API' });
});

// Main endpoint: Identify plant from uploaded image
app.post('/api/identify', upload.single('image'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No image file provided' });
    }

    const organ = req.body.organ || 'leaf'; // leaf, flower, fruit, bark
    const project = req.body.project || 'useful'; // useful = Asian/tropical plants

    // Send to Pl@ntNet
    const form = new FormData();
    form.append('images', fs.createReadStream(req.file.path));
    form.append('organs', organ);

    const response = await axios.post(
      `https://my-api.plantnet.org/v2/identify/${project}?api-key=${PLANTNET_API_KEY}&include-related-images=true`,
      form,
      {
        headers: form.getHeaders()
      }
    );

    // Clean up uploaded file
    fs.unlinkSync(req.file.path);

    // Format response for EcoDex
    const ecodexData = formatEcoDexResponse(response.data);
    
    res.json(ecodexData);

  } catch (error) {
    // Clean up file on error
    if (req.file) {
      fs.unlinkSync(req.file.path);
    }

    console.error('Identification error:', error.response?.data || error.message);
    res.status(500).json({
      error: 'Plant identification failed',
      message: error.response?.data?.message || error.message
    });
  }
});

// Endpoint: Identify plant from raw image data (for ESP32-CAM)
app.post('/api/identify-raw', async (req, res) => {
  try {
    // ESP32 sends raw JPEG data in body
    if (!req.body || req.body.length === 0) {
      return res.status(400).json({ error: 'No image data provided' });
    }

    const organ = req.query.organ || 'leaf';
    const project = req.query.project || 'useful';

    // Save temporary file
    const tempPath = `uploads/esp32-${Date.now()}.jpg`;
    fs.writeFileSync(tempPath, req.body);

    // Send to Pl@ntNet
    const form = new FormData();
    form.append('images', fs.createReadStream(tempPath));
    form.append('organs', organ);

    const response = await axios.post(
      `https://my-api.plantnet.org/v2/identify/${project}?api-key=${PLANTNET_API_KEY}&include-related-images=true`,
      form,
      {
        headers: form.getHeaders()
      }
    );

    // Clean up temp file
    fs.unlinkSync(tempPath);

    // Format response for EcoDex
    const ecodexData = formatEcoDexResponse(response.data);
    
    res.json(ecodexData);

  } catch (error) {
    console.error('Identification error:', error.response?.data || error.message);
    res.status(500).json({
      error: 'Plant identification failed',
      message: error.response?.data?.message || error.message
    });
  }
});

// Endpoint: Identify plant from image URL (for IoT devices)
app.post('/api/identify-url', async (req, res) => {
  try {
    const { imageUrl, organ = 'leaf', project = 'useful' } = req.body;

    if (!imageUrl) {
      return res.status(400).json({ error: 'No image URL provided' });
    }

    const response = await axios.get(
      `https://my-api.plantnet.org/v2/identify/${project}`,
      {
        params: {
          'api-key': PLANTNET_API_KEY,
          'images': imageUrl,
          'organs': organ,
          'include-related-images': 'true'
        }
      }
    );

    const ecodexData = formatEcoDexResponse(response.data);
    res.json(ecodexData);

  } catch (error) {
    console.error('Identification error:', error.response?.data || error.message);
    res.status(500).json({
      error: 'Plant identification failed',
      message: error.response?.data?.message || error.message
    });
  }
});

// Endpoint: Get plant details by scientific name
app.get('/api/plant/:scientificName', async (req, res) => {
  try {
    // This would connect to your database where you store captured plants
    // For now, return mock data
    res.json({
      scientificName: req.params.scientificName,
      captured: false,
      captureDate: null,
      location: null,
      notes: null
    });
  } catch (error) {
    res.status(500).json({ error: 'Failed to fetch plant details' });
  }
});

// Format Pl@ntNet response for EcoDex
function formatEcoDexResponse(plantnetData) {
  const topResults = plantnetData.results.slice(0, 5); // Top 5 matches

  return {
    success: true,
    timestamp: new Date().toISOString(),
    query: {
      project: plantnetData.query.project,
      organ: plantnetData.query.organs[0]
    },
    plants: topResults.map((result, index) => ({
      rank: index + 1,
      scientificName: result.species.scientificNameWithoutAuthor,
      scientificNameAuthored: result.species.scientificName,
      commonNames: result.species.commonNames || [],
      family: {
        scientific: result.species.family.scientificNameWithoutAuthor,
        common: result.species.family.commonNames?.[0] || null
      },
      genus: result.species.genus.scientificNameWithoutAuthor,
      confidence: Math.round(result.score * 100),
      images: result.images?.slice(0, 3).map(img => ({
        url: img.url.o,
        thumbnail: img.url.s,
        organ: img.organ
      })) || [],
      gbifId: result.gbif?.id || null,
      // EcoDex game mechanics
      captured: false,
      rarity: getRarity(result.score),
      pokedexNumber: null,
      // Additional info
      description: `A ${result.species.family.scientificNameWithoutAuthor} plant commonly found in tropical regions.`
    })),
    meta: {
      remainingIdentifications: plantnetData.remainingIdentificationRequests,
      apiVersion: plantnetData.version,
      bestMatch: topResults[0]?.species.scientificNameWithoutAuthor || null,
      bestMatchConfidence: topResults[0] ? Math.round(topResults[0].score * 100) : 0
    }
  };
}

function getRarity(score) {
  if (score > 0.8) return 'Common';
  if (score > 0.5) return 'Uncommon';
  if (score > 0.3) return 'Rare';
  return 'Ultra Rare';
}

// Start server
app.listen(PORT, () => {
  console.log(`🌿 EcoDex API Server running on port ${PORT}`);
  console.log(`📡 Endpoints:`);
  console.log(`   POST /api/identify - Upload image for identification`);
  console.log(`   POST /api/identify-url - Identify from image URL`);
  console.log(`   GET  /api/plant/:name - Get plant details`);
});

module.exports = app;
