# Arduino IDE Display Test

## Setup Instructions

1. **Install TFT_eSPI library in Arduino IDE:**
   - Open Arduino IDE
   - Go to Tools > Manage Libraries
   - Search for "TFT_eSPI" by Bodmer
   - Click Install

2. **Configure TFT_eSPI library:**
   - After installation, navigate to the library folder:
     `Documents\Arduino\libraries\TFT_eSPI`
   - Open `User_Setup.h` file
   - Replace the entire content with the User_Setup.h from this folder
   - Save and close

3. **Connect CYD board:**
   - Connect CYD-2432S028 to USB
   - In Arduino IDE, select Board: "ESP32 Dev Module"
   - Select correct COM port
   - Set Upload Speed: 921600

4. **Upload the test:**
   - Open `display_test.ino`
   - Click Upload
   - Open Serial Monitor (115200 baud)

## Expected Results

You should see:
- Three vertical color bars (Red, Green, Blue)
- Text: "ARDUINO TEST WORKING!"
- Serial Monitor output showing display initialization

## If Still White Screen

The display is likely hardware damaged. Try:
1. Check ribbon cable connection
2. Replace display module
3. Test with another CYD board

## Pin Configuration

Using your original pins:
- MOSI: 13
- SCLK: 14
- CS: 15
- DC: 2
- Backlight: 21
- Touch CS: 33
