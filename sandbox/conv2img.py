from PIL import Image, ImageDraw, ImageFont

def text_to_image(text, width, height, font_size=20):
    image = Image.new('1', (width, height), color=255)  # '1' for 1-bit color (black & white)
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype("arial.ttf", font_size)  # Change to a monospaced font for consistency
    draw.multiline_text((10, 10), text, fill=0, font=font)  # Fill=0 for black text
    return image

text = "This is a sample text to display on an e-ink screen."
image = text_to_image(text, 200, 300)  # Adjust dimensions to your e-paper size
image.save("text_image.png")
