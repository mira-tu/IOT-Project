// Pl@ntNet API - Node.js Example for EcoDex
// Install: npm install axios form-data

const axios = require('axios');
const FormData = require('form-data');
const fs = require('fs');

// Your API key from my.plantnet.org
const API_KEY = 'YOUR_API_KEY_HERE';

// Method 1: Identify plant from local image file
async function identifyPlantFromFile(imagePath) {
  try {
    const form = new FormData();
    form.append('images', fs.createReadStream(imagePath));
    form.append('organs', 'leaf'); // or 'flower', 'fruit', 'bark'

    const response = await axios.post(
      `https://my-api.plantnet.org/v2/identify/useful?api-key=${API_KEY}`,
      form,
      {
        headers: form.getHeaders()
      }
    );

    return formatEcoDexResponse(response.data);
  } catch (error) {
    console.error('Error:', error.response?.data || error.message);
    throw error;
  }
}

// Method 2: Identify plant from image URL
async function identifyPlantFromURL(imageUrl) {
  try {
    const response = await axios.get(
      `https://my-api.plantnet.org/v2/identify/useful`,
      {
        params: {
          'api-key': API_KEY,
          'images': imageUrl,
          'organs': 'leaf'
        }
      }
    );

    return formatEcoDexResponse(response.data);
  } catch (error) {
    console.error('Error:', error.response?.data || error.message);
    throw error;
  }
}

// Method 3: With Philippine location filtering
async function identifyPhilippinePlant(imagePath, latitude = 14.5995, longitude = 120.9842) {
  try {
    const form = new FormData();
    form.append('images', fs.createReadStream(imagePath));
    form.append('organs', 'leaf');
    
    // Note: Pl@ntNet doesn't have direct lat/long filtering
    // But 'useful' project includes Asian tropical plants
    const response = await axios.post(
      `https://my-api.plantnet.org/v2/identify/useful?api-key=${API_KEY}&include-related-images=true`,
      form,
      {
        headers: form.getHeaders()
      }
    );

    return formatEcoDexResponse(response.data);
  } catch (error) {
    console.error('Error:', error.response?.data || error.message);
    throw error;
  }
}

// Format response in PokeDex style for EcoDex
function formatEcoDexResponse(plantnetData) {
  const topResults = plantnetData.results.slice(0, 3); // Top 3 matches

  return {
    success: true,
    query: {
      project: plantnetData.query.project,
      organ: plantnetData.query.organs[0]
    },
    plants: topResults.map((result, index) => ({
      rank: index + 1,
      scientificName: result.species.scientificNameWithoutAuthor,
      commonNames: result.species.commonNames || [],
      family: result.species.family.scientificNameWithoutAuthor,
      genus: result.species.genus.scientificNameWithoutAuthor,
      confidence: Math.round(result.score * 100), // Convert to percentage
      images: result.images?.slice(0, 3).map(img => img.url.o) || [],
      gbifId: result.gbif?.id || null,
      // EcoDex specific fields
      captured: false,
      rarity: getRarity(result.score),
      pokedexNumber: null // You can assign your own numbering
    })),
    remainingIdentifications: plantnetData.remainingIdentificationRequests,
    version: plantnetData.version
  };
}

// Determine rarity based on confidence (like Pokemon)
function getRarity(score) {
  if (score > 0.8) return 'Common';
  if (score > 0.5) return 'Uncommon';
  if (score > 0.3) return 'Rare';
  return 'Ultra Rare';
}

// Example usage
async function main() {
  try {
    // Example 1: From local file
    console.log('Identifying plant from file...');
    const result1 = await identifyPlantFromFile('./plant-photo.jpg');
    console.log(JSON.stringify(result1, null, 2));

    // Example 2: From URL
    console.log('\nIdentifying plant from URL...');
    const result2 = await identifyPlantFromURL(
      'https://example.com/plant-image.jpg'
    );
    console.log(JSON.stringify(result2, null, 2));

  } catch (error) {
    console.error('Failed:', error.message);
  }
}

// Uncomment to run
// main();

module.exports = {
  identifyPlantFromFile,
  identifyPlantFromURL,
  identifyPhilippinePlant
};
