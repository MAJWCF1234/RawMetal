import json
import random
import subprocess
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk
from PIL import Image, ImageTk
from urllib.error import URLError
from urllib.request import Request, urlopen
from pathlib import Path


APP = Path.home() / 'AppData/Local/ComfyUI/app'
PYTHON = APP.parent / '.venv/Scripts/python.exe'
SERVER = 'http://127.0.0.1:8189'
MODEL = 'sd_xl_base_1.0.safetensors'


def request(path, data=None):
    body = None if data is None else json.dumps(data).encode()
    req = Request(SERVER + path, body, {'Content-Type': 'application/json'})
    with urlopen(req, timeout=10) as response:
        return json.load(response)


def start_server():
    try:
        request('/system_stats')
        return
    except (OSError, URLError):
        pass
    subprocess.Popen([str(PYTHON), '-s', 'main.py', '--listen', '127.0.0.1', '--port', '8189'],
                     cwd=APP, creationflags=subprocess.CREATE_NO_WINDOW,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(120):
        time.sleep(1)
        try:
            request('/system_stats')
            return
        except (OSError, URLError):
            continue
    raise RuntimeError('ComfyUI did not start within two minutes.')


def workflow(description, seed):
    prompt = ('A single continuous realistic architectural surface photographed straight on, '
              'uniform scale, physically plausible fine material detail, muted natural colors, '
              'even diffuse lighting, no perspective, game environment material: ' + description)
    nodes = {
        '1': {'class_type': 'CheckpointLoaderSimple', 'inputs': {'ckpt_name': MODEL}},
        '2': {'class_type': 'CLIPTextEncode', 'inputs': {'text': prompt, 'clip': ['1', 1]}},
        '3': {'class_type': 'CLIPTextEncode', 'inputs': {'text': 'text, letters, collage, grid of images, panels, borders, frames, seams, perspective, harsh lighting, oversaturated, cartoon, watermark', 'clip': ['1', 1]}},
        '4': {'class_type': 'EmptyLatentImage', 'inputs': {'width': 1024, 'height': 1024, 'batch_size': 1}},
        '13': {'class_type': 'SeamlessTile', 'inputs': {'model': ['1', 0], 'tiling': 'enable', 'copy_model': 'Make a copy'}},
        '14': {'class_type': 'MakeCircularVAE', 'inputs': {'vae': ['1', 2], 'tiling': 'enable', 'copy_vae': 'Make a copy'}},
        '5': {'class_type': 'KSampler', 'inputs': {'model': ['13', 0], 'positive': ['2', 0], 'negative': ['3', 0], 'latent_image': ['4', 0], 'seed': seed, 'steps': 30, 'cfg': 6.0, 'sampler_name': 'dpmpp_2m', 'scheduler': 'karras', 'denoise': 1.0}},
        '6': {'class_type': 'VAEDecode', 'inputs': {'samples': ['5', 0], 'vae': ['14', 0]}},
        '8': {'class_type': 'TextureNormalFromHeight', 'inputs': {'height': ['6', 0], 'strength': 2.0, 'blur_px': 1, 'convention': 'OpenGL (+Y)', 'wrap_edges': True}},
        '9': {'class_type': 'TextureAOFromHeight', 'inputs': {'height': ['6', 0], 'strength': 2.0, 'radius_px': 12, 'directions': '8', 'wrap_edges': True}},
    }
    folder = time.strftime('RawMetal/%Y%m%d-%H%M%S')
    for number, label, source in [('10', 'color', '6'), ('11', 'normal', '8'), ('12', 'ao', '9')]:
        nodes[number] = {'class_type': 'SaveImage', 'inputs': {'images': [source, 0], 'filename_prefix': folder + '/' + label}}
    return nodes, folder


def generate():
    description = prompt.get().strip()
    if not description:
        messagebox.showinfo('Texture Maker', 'Enter a description first.')
        return
    button.config(state='disabled')
    status.set('Starting ComfyUI…')

    def work():
        try:
            if not (APP / 'models/checkpoints' / MODEL).exists():
                raise RuntimeError('The Stable Diffusion checkpoint is missing.')
            start_server()
            graph, folder = workflow(description, random.randrange(2**32))
            result = request('/prompt', {'prompt': graph})
            job = result['prompt_id']
            root.after(0, status.set, 'Generating texture and maps…')
            while True:
                time.sleep(2)
                history = request('/history/' + job)
                if job in history:
                    item = history[job]
                    if item.get('status', {}).get('status_str') == 'error':
                        raise RuntimeError(str(item['status'].get('messages', 'Generation failed.')))
                    if item.get('outputs'):
                        break
            output = APP / 'output' / folder
            colors = sorted(output.glob('color_*.png'))
            if not colors:
                raise RuntimeError('ComfyUI finished without a color image.')
            def show_result():
                image = Image.open(colors[0]).convert('RGB')
                image.thumbnail((300, 300))
                preview.image = ImageTk.PhotoImage(image)
                preview.config(image=preview.image)
                status.set('Saved to ' + str(output))
                open_button.config(state='normal', command=lambda: subprocess.Popen(['explorer', str(output)]))
            root.after(0, show_result)
        except Exception as error:
            root.after(0, status.set, 'Generation failed.')
            root.after(0, lambda message=str(error): messagebox.showerror('Texture Maker', message))
        finally:
            root.after(0, lambda: button.config(state='normal'))

    threading.Thread(target=work, daemon=True).start()


root = tk.Tk()
root.title('RawMetal Texture Maker')
root.geometry('620x585')
root.resizable(False, False)
frame = ttk.Frame(root, padding=24)
frame.pack(fill='both', expand=True)
ttk.Label(frame, text='Describe a surface', font=('Segoe UI', 15, 'bold')).pack(anchor='w')
prompt = ttk.Entry(frame, font=('Segoe UI', 12))
prompt.insert(0, 'weathered old red-brown brick wall, small rectangular bricks arranged in regular straight staggered horizontal rows, fine mortar joints, thin dark green moss in the mortar, flat surface')
prompt.pack(fill='x', pady=(12, 12))
button = ttk.Button(frame, text='Generate texture', command=generate)
button.pack(anchor='w')
open_button = ttk.Button(frame, text='Open output folder', state='disabled')
open_button.pack(anchor='w', pady=(8, 0))
preview = ttk.Label(frame)
preview.pack(anchor='w', pady=(12, 0))
status = tk.StringVar(value='Creates a tiled color image, approximate normal and ambient occlusion maps.')
ttk.Label(frame, textvariable=status, wraplength=560).pack(anchor='w', pady=(18, 0))
root.mainloop()
