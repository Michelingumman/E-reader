
from epub_conversion import Converter
import os

input_book = "sandbox/books/2_b_converted/"
output = "converted.gz"


converted = Converter(input_book) # Convert the book
converted.convert("converted.gz") # store as .text