import argparse
from PIL import Image
import os
import sys

def convert_image(input_path, output_path, width, height, var_name):
    # 1. Open image
    try:
        img = Image.open(input_path)
        img = img.convert('RGBA')
    except Exception as e:
        print(f'Error opening image: {e}')
        return

    # 2. Determine target size
    if width is None and height is None:
        target_size = img.size
    elif width is not None and height is not None:
        target_size = (width, height)
    elif width is not None:
        # Scale height proportionally
        ratio = width / img.width
        target_size = (width, int(img.height * ratio))
    else:
        # Scale width proportionally
        ratio = height / img.height
        target_size = (int(img.width * ratio), height)

    print(f"Converting '{input_path}' to '{output_path}'")
    print(f"Original Size: {img.size}, Target Size: {target_size}")

    # 3. Resize maintaining aspect ratio and center on transparent canvas
    # Create transparent canvas
    canvas = Image.new('RGBA', target_size, (0, 0, 0, 0))
    
    # Resize image to fit within target_size while maintaining aspect ratio
    img.thumbnail(target_size, Image.Resampling.LANCZOS)
    
    # Center image on canvas
    x = (target_size[0] - img.width) // 2
    y = (target_size[1] - img.height) // 2
    canvas.paste(img, (x, y), img)

    # 4. Generate C code
    c_content = []
    c_content.append('#include "lvgl.h"') # Adjust include path if needed
    c_content.append('')
    c_content.append('#ifndef LV_ATTRIBUTE_MEM_ALIGN')
    c_content.append('#define LV_ATTRIBUTE_MEM_ALIGN')
    c_content.append('#endif')
    c_content.append('')
    c_content.append(f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t {var_name}_map[] = {{')
    
    data = list(canvas.getdata())
    line_data = []
    
    for i, pixel in enumerate(data):
        r, g, b, a = pixel
        # LVGL ARGB8888 format: Blue, Green, Red, Alpha
        line_data.append(f'0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x}')
        
        if len(line_data) >= 4: # 4 pixels per line
            c_content.append('  ' + ', '.join(line_data) + ',')
            line_data = []
            
    if line_data:
        c_content.append('  ' + ', '.join(line_data) + ',')

    c_content.append('};')
    c_content.append('')
    c_content.append(f'const lv_image_dsc_t {var_name} = {{')
    c_content.append('  .header.cf = LV_COLOR_FORMAT_ARGB8888,')
    c_content.append('  .header.magic = LV_IMAGE_HEADER_MAGIC,')
    c_content.append(f'  .header.w = {target_size[0]},')
    c_content.append(f'  .header.h = {target_size[1]},')
    c_content.append(f'  .data_size = {target_size[0] * target_size[1] * 4},')
    c_content.append(f'  .data = {var_name}_map,')
    c_content.append('};')

    # 5. Write to file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(c_content))
        
    print('Done!')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert image to LVGL ARGB8888 C array.')
    parser.add_argument('input', help='Input image path')
    parser.add_argument('output', help='Output C file path')
    parser.add_argument('--width', type=int, help='Target width')
    parser.add_argument('--height', type=int, help='Target height')
    parser.add_argument('--name', default='my_image', help='Variable name in C code')

    args = parser.parse_args()
    
    convert_image(args.input, args.output, args.width, args.height, args.name)
