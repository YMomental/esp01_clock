import re

# Твій масив
c_code = """
const byte FONT_5x8[10][5] = {
  {0x7E, 0x81, 0x81, 0x81, 0x7E}, // 0
  {0x00, 0x04, 0xFF, 0x00, 0x00}, // 1
  {0xC2, 0xA1, 0x91, 0x89, 0x86}, // 2
  {0x42, 0x81, 0x89, 0x89, 0x76}, // 3
  {0x18, 0x14, 0x12, 0xFF, 0x10}, // 4
  {0x4F, 0x89, 0x89, 0x89, 0x71}, // 5
  {0x7E, 0x89, 0x89, 0x89, 0x70}, // 6
  {0xC1, 0x31, 0x09, 0x05, 0x03}, // 7
  {0x76, 0x89, 0x89, 0x89, 0x76}, // 8
  {0x4E, 0x91, 0x91, 0x91, 0x7E}  // 9
};
"""

# Витягуємо всі байти
hex_values = re.findall(r'0x[0-9A-Fa-f]{2}', c_code)
bytes_list = [int(h, 16) for h in hex_values]

# Групуємо по 5 байтів на символ
chars = [bytes_list[i:i+5] for i in range(0, len(bytes_list), 5)]
print("1")
# Рендеринг в один рядок
# Спочатку йдемо по висоті (8 рядків зверху вниз)
for bit in range(8):
    line_str = ""
    
    # Для кожного рядка малюємо відповідну частину КОЖНОЇ цифри
    for char_bytes in chars:
        for col in range(5):
            if char_bytes[col] & (1 << bit):
                line_str += "██"
            else:
                line_str += "  "
                
        # Робимо відступ між цифрами (3 пробіли)
        line_str += "   "
        
    # Друкуємо сформований рядок з усіма шматками цифр і переходимо на новий
    print(line_str)