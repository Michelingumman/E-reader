import fitz  # PyMuPDF
import json
import os
import re  # For regex matching

input_pdf = "Beyond-Order.pdf"
pdf_base_name = os.path.splitext(os.path.basename(input_pdf))[0]

# Function to extract and store PDF data with chapter detection
def extract_and_store_pdf_data(output_json_path):
    pdf_data = {
        pdf_base_name: []  # Initialize the book with an empty list of pages
    }

    with fitz.open(input_pdf) as pdf:
        for page_num in range(pdf.page_count):
            page = pdf[page_num]
            text = page.get_text("text")  # Extract text from page

            # Add page data with chapter and content
            pdf_data[pdf_base_name].append({
                'page_number': page_num + 1,
                'content': text
            })

    # Write the structured data to a JSON file
    with open(output_json_path, 'w', encoding='utf-8') as json_file:
        json.dump(pdf_data, json_file, ensure_ascii=False, indent=4)

# Example usage
extract_and_store_pdf_data(f'OUTPUT/{pdf_base_name}.json')
print(f"Text and page data saved to OUTPUT/{pdf_base_name}.json")
