"""
Pl@ntNet API - Python Example for EcoDex
Install: pip install requests
"""

import requests
import json

# Your API key from my.plantnet.org
API_KEY = 'YOUR_API_KEY_HERE'

def identify_plant_from_file(image_path, organ='leaf'):
    """
    Identify plant from local image file
    
    Args:
        image_path: Path to image file
        organ: Plant part - 'leaf', 'flower', 'fruit', or 'bark'
    """
    url = f'https://my-api.plantnet.org/v2/identify/useful?api-key={API_KEY}'
    
    with open(image_path, 'rb') as image_file:
        files = {'images': image_file}
        data = {'organs': organ}
        
        response = requests.post(url, files=files, data=data)
        response.raise_for_status()
        
        return format_ecodex_response(response.json())

def identify_plant_from_url(image_url, organ='leaf'):
    """
    Identify plant from image URL
    
    Args:
        image_url: URL of the image
        organ: Plant part - 'leaf', 'flower', 'fruit', or 'bark'
    """
    url = 'https://my-api.plantnet.org/v2/identify/useful'
    
    params = {
        'api-key': API_KEY,
        'images': image_url,
        'organs': organ,
        'include-related-images': 'true'
    }
    
    response = requests.get(url, params=params)
    response.raise_for_status()
    
    return format_ecodex_response(response.json())

def identify_philippine_plant(image_path, organ='leaf'):
    """
    Identify plant with focus on Philippine/Asian tropical plants
    Uses 'useful' project which includes Asian flora
    
    Args:
        image_path: Path to image file
        organ: Plant part - 'leaf', 'flower', 'fruit', or 'bark'
    """
    # Using 'useful' project for Asian/tropical plants
    url = f'https://my-api.plantnet.org/v2/identify/useful?api-key={API_KEY}&include-related-images=true'
    
    with open(image_path, 'rb') as image_file:
        files = {'images': image_file}
        data = {'organs': organ}
        
        response = requests.post(url, files=files, data=data)
        response.raise_for_status()
        
        return format_ecodex_response(response.json())

def format_ecodex_response(plantnet_data):
    """
    Format Pl@ntNet response in PokeDex style for EcoDex app
    """
    top_results = plantnet_data['results'][:3]  # Top 3 matches
    
    ecodex_response = {
        'success': True,
        'query': {
            'project': plantnet_data['query']['project'],
            'organ': plantnet_data['query']['organs'][0] if plantnet_data['query']['organs'] else None
        },
        'plants': []
    }
    
    for index, result in enumerate(top_results):
        plant = {
            'rank': index + 1,
            'scientificName': result['species']['scientificNameWithoutAuthor'],
            'commonNames': result['species'].get('commonNames', []),
            'family': result['species']['family']['scientificNameWithoutAuthor'],
            'genus': result['species']['genus']['scientificNameWithoutAuthor'],
            'confidence': round(result['score'] * 100),  # Convert to percentage
            'images': [img['url']['o'] for img in result.get('images', [])[:3]],
            'gbifId': result.get('gbif', {}).get('id'),
            # EcoDex specific fields
            'captured': False,
            'rarity': get_rarity(result['score']),
            'pokedexNumber': None  # Assign your own numbering system
        }
        ecodex_response['plants'].append(plant)
    
    ecodex_response['remainingIdentifications'] = plantnet_data.get('remainingIdentificationRequests')
    ecodex_response['version'] = plantnet_data.get('version')
    
    return ecodex_response

def get_rarity(score):
    """
    Determine rarity based on confidence score (like Pokemon rarity)
    """
    if score > 0.8:
        return 'Common'
    elif score > 0.5:
        return 'Uncommon'
    elif score > 0.3:
        return 'Rare'
    else:
        return 'Ultra Rare'

# Example usage
if __name__ == '__main__':
    try:
        # Example 1: From local file
        print('Identifying plant from file...')
        result = identify_plant_from_file('./plant-photo.jpg', organ='leaf')
        print(json.dumps(result, indent=2))
        
        # Example 2: From URL
        print('\nIdentifying plant from URL...')
        result = identify_plant_from_url(
            'https://example.com/plant-image.jpg',
            organ='flower'
        )
        print(json.dumps(result, indent=2))
        
        # Display top result in EcoDex style
        if result['plants']:
            top_plant = result['plants'][0]
            print(f"\n🌿 EcoDex Entry #{top_plant['rank']}")
            print(f"Name: {top_plant['scientificName']}")
            print(f"Common: {', '.join(top_plant['commonNames'][:3])}")
            print(f"Family: {top_plant['family']}")
            print(f"Confidence: {top_plant['confidence']}%")
            print(f"Rarity: {top_plant['rarity']}")
            
    except requests.exceptions.RequestException as e:
        print(f'Error: {e}')
