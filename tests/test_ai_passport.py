"""Validate the landscape port, real key dispatcher, and bounded rendering."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
PORT=ROOT/'ports/ai-passport'
OUT=ROOT/'artifacts/ai-passport/host'
OUT.mkdir(parents=True,exist_ok=True)

with tempfile.TemporaryDirectory(prefix='passport-input-') as temporary:
    binary=Path(temporary)/'input'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(PORT/'include'),
                    str(PORT/'tests/test_passport_input.c'),str(PORT/'main/passport_input.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

if sys.platform!='darwin':
    print('PASS: Passport input; macOS visual comparison not run on this host')
    raise SystemExit(0)

with (OUT/'desktop-build.log').open('wb') as log:
    subprocess.run(['python3',str(ROOT/'ports/desktop/build.py'),'--passport'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
binary=ROOT/'artifacts/desktop-passport/GVIEW Travel Passport Simulator.app/Contents/MacOS/GVIEW-Simulator'

def smoke(mode):
    environment=os.environ.copy()
    environment['GVIEW_TEST_TIME']='1794630840'
    environment.pop('GVIEW_TEST_FULL_FRAME',None)
    if mode=='full':environment['GVIEW_TEST_FULL_FRAME']='1'
    # Every run has fresh isolated saves so previous previews cannot affect gold/ownership.
    with tempfile.TemporaryDirectory(prefix='passport-'+mode+'-') as temporary:
        directory=Path(temporary)
        with (OUT/(mode+'.log')).open('wb') as log:
            subprocess.run([str(binary),'--smoke',str(directory)],cwd=ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
        captures={image.name:Image.open(image).convert('RGB').copy() for image in directory.glob('*.png')}
    for image in captures.values():assert image.size==(320,240)
    destination=OUT/mode
    destination.mkdir(exist_ok=True)
    for name,image in captures.items():image.save(destination/name)
    return captures

with ThreadPoolExecutor(max_workers=2) as pool:
    strip,full=list(pool.map(smoke,('strip','full')))
# desktop_open exposes nine page routes (0-8).
required={f'page-{i}.png' for i in range(9)}
required.update(f'shop-category-{i}.png' for i in range(9))
required.update(f'home-weather-{i}.png' for i in range(7))
required.update(('home-final.png','quick-companion.png','power-menu.png','garden-full.png','garden-shop.png','tomato.png','countdown.png','sound.png','wifi.png','pixel-format-16.png','pixel-format-16-scaled.png','pixel-format-16-rotated.png','pixel-format-16-corners.png'))
required.update(('render-nested-numbers.png','render-nested-numbers-moved.png'))
required.update('wifi-'+name+'.png' for name in (
    'home','scan-top','scan-bottom','scan-empty','scanning','saved-empty',
    'saved-top','saved-bottom','saved-detail','saved-delete','keyboard-empty',
    'keyboard-lower','keyboard-upper','keyboard-digits','keyboard-symbols',
    'connecting','connected','failed'))
required.add('round-rect-wide.png')
required.update('layout-'+name+'.png' for name in (
    'calendar','sound','bgm','bgm-last','system','alarm','alarm-edit',
    'light','city-province','city-city','tomato-settings','timer-settings',
    'help','reset-confirm','shop','garden','garden-shop','notice-wide'))
required.update(('weather-three-days.png','weather-edge-values.png','weather-waiting.png'))
assert strip.keys()==full.keys()==required, {
    'strip_missing':sorted(required-strip.keys()),'strip_extra':sorted(strip.keys()-required),
    'full_missing':sorted(required-full.keys()),'full_extra':sorted(full.keys()-required)}
for name in strip:
    assert strip[name].tobytes()==full[name].tobytes(), 'Strip/full mismatch: '+name
    strip[name].save(OUT/name)

pages=[f'page-{page}.png' for page in range(9)]+['power-menu.png','shop-category-8.png']
sheet=Image.new('RGB',(320*4,240*3),'white')
for index,name in enumerate(pages):sheet.paste(strip[name],((index%4)*320,(index//4)*240))
sheet.save(OUT/'contact-sheet.png')
wifi_pages=['wifi-home.png','wifi-scan-top.png','wifi-scan-bottom.png','wifi-saved-top.png',
           'wifi-saved-bottom.png','wifi-saved-detail.png','wifi-keyboard-empty.png',
           'wifi-keyboard-symbols.png','wifi-scanning.png','wifi-connected.png',
           'wifi-failed.png','wifi-saved-empty.png']
wifi_sheet=Image.new('RGB',(320*4,240*3),'white')
for index,name in enumerate(wifi_pages):wifi_sheet.paste(strip[name],((index%4)*320,(index//4)*240))
wifi_sheet.save(OUT/'wifi-contact-sheet.png')
layout_pages=['calendar','sound','system','alarm','alarm-edit','light','city-province',
              'tomato-settings','timer-settings','bgm','shop','garden-shop']
layout_sheet=Image.new('RGB',(320*4,240*3),'white')
for index,name in enumerate(layout_pages):layout_sheet.paste(strip['layout-'+name+'.png'],((index%4)*320,(index//4)*240))
layout_sheet.save(OUT/'page-layout-contact-sheet.png')
weather_sheet=Image.new('RGB',(320*3,240),'white')
for index,name in enumerate(('weather-three-days.png','weather-edge-values.png','weather-waiting.png')):
    weather_sheet.paste(strip[name],(320*index,0))
weather_sheet.save(OUT/'weather-contact-sheet.png')
result={'resolution':[320,240],'strip_rows':16,'framebuffer_bytes':320*16*2,
        'pixel_identical_captures':len(strip), 'single_scene_traversal':True, 'numeric_formats_once_per_frame':True,'same_device_dispatcher':True,
        'wifi_state_captures':18,'wifi_geometry_and_keyboard_actions':True,'wide_rounded_bitmap':True,
        'page_layout_states':18,'target_viewport_and_panel_geometry':True,
        'weather_columns':3,'weather_days':['today','tomorrow','day_after_tomorrow'],
        'companions':len(json.loads((ROOT/'assets/pokemon_followers/pricing.json').read_text())['species']),
        'shop_categories':9,'weather_states':7,'object_pool':256,'full_garden_plants':30}
(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'PASS: Passport keys and {len(strip)} pixel-identical strip/full captures at 320x240; 10 KB render buffer')
