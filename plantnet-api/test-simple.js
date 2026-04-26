// Simple test to verify Pl@ntNet API key works
require('dotenv').config();
const axios = require('axios');

const API_KEY = process.env.PLANTNET_API_KEY;

console.log('🌿 Testing Pl@ntNet API Connection...\n');
console.log('API Key:', API_KEY ? `${API_KEY.substring(0, 10)}...` : '❌ NOT FOUND');
console.log('');

// Test with a real publicly accessible plant image
const testImageUrl = 'https://upload.wikimedia.org/wikipedia/commons/thumb/4/41/Sunflower_from_Silesia2.jpg/800px-Sunflower_from_Silesia2.jpg';

async function testAPI() {
  try {
    console.log('Sending test request to Pl@ntNet...');
    
    const response = await axios.get(
      'https://my-api.plantnet.org/v2/identify/useful',
      {
        params: {
          'api-key': API_KEY,
          'images': testImageUrl,
          'organs': 'leaf'
        },
        timeout: 10000
      }
    );

    console.log('✅ SUCCESS! API is working!\n');
    console.log('Response Summary:');
    console.log('─────────────────────────────────────');
    console.log(`Project: ${response.data.query.project}`);
    console.log(`Results found: ${response.data.results.length}`);
    console.log(`Remaining IDs today: ${response.data.remainingIdentificationRequests}`);
    
    if (response.data.results.length > 0) {
      const topResult = response.data.results[0];
      console.log('\n🌱 Top Match:');
      console.log(`   Scientific: ${topResult.species.scientificNameWithoutAuthor}`);
      console.log(`   Common: ${topResult.species.commonNames?.join(', ') || 'N/A'}`);
      console.log(`   Family: ${topResult.species.family.scientificNameWithoutAuthor}`);
      console.log(`   Confidence: ${Math.round(topResult.score * 100)}%`);
    }
    
    console.log('\n✅ Your EcoDex API is ready to use!');
    console.log('   Run: npm start');
    
  } catch (error) {
    console.error('❌ ERROR:', error.response?.data?.message || error.message);
    
    if (error.response?.status === 401) {
      console.error('\n⚠️  Invalid API key. Please check your .env file');
    } else if (error.response?.status === 404) {
      console.error('\n⚠️  API endpoint not found. Check the URL');
    } else if (error.code === 'ECONNABORTED') {
      console.error('\n⚠️  Request timeout. Check your internet connection');
    }
  }
}

testAPI();
