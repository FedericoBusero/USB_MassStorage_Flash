/*
  Tinyusb example msc_internal_flash_samd / msc_external_flash

    Settings Seeduino XIAO
    ==>Menu: Tools>USB Stack: tinyusb
    Core: Seeed SAMD boards: 1.8.3
    ==>Tinyusb library: 0.10.5 (oude versie !!)
    SdFat.h : adafruit version 2.2.1
    Adafruit Internalflash versie 0.1.0 (arduino ide versie) https://github.com/adafruit/Adafruit_InternalFlash
    FlashStorage by Arduino versie 1.0.0 vanuit IDE ofwel vanuit github https://github.com/cmaglie/FlashStorage ??

    Settings Adafruit QT PY
    ==>Menu: Tools>USB Stack:TinyUSB
    Core: laatste versie Adafruit SAMD 1.7.11
    ==>Adafruit TinyUSB: laatste versie (1.16.0/1.17.0)
    Adafruit Internalflash versie 0.1.0
    SdFat.h : adafruit version 2.2.1
    FlashStorage by Arduino versie 1.0.0 vanuit IDE ofwel vanuit github https://github.com/cmaglie/FlashStorage

    Settings Lolin S2 Mini (ESP32-S2 )
    Board: Lolin S2 Mini
    USB CDC On Boot: enabled
    USB Firmware MSC On Boot: Disabled
    USB DFU On Boot: Disabled
    ==>Partition Scheme: Default 4MB with ffat (1.2 MB App/1.5MB FATFS)
    Core Debug Level: Geen
    Erase All Flash before Sketch upload: disabled

    ESP32 Arduino core: 2.0.5
    Library
    - SdFat: Adafruit fork 2.2.1
    - Adafruit SPIFlash : 4.0.0
    - TinyUSB: 1.16.0 (NOT 1.17.0 !!)
*/

/*********************************************************************
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  MIT license, check LICENSE for more information
  Copyright (c) 2019 Ha Thach for Adafruit Industries
  All text above, and the splash screen below must be included in
  any redistribution
*********************************************************************/

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_TinyUSB.h"

#ifdef ARDUINO_ARCH_ESP32
#include "Adafruit_SPIFlash.h"

Adafruit_FlashTransport_ESP32 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

#define LED_PIN LED_BUILTIN
#define LED_ON HIGH
#define LED_OFF LOW

#else // not ARDUINO_ARCH_ESP32 -> SAMD21

#include "Adafruit_InternalFlash.h"

// Start address and size should matches value in the CircuitPython (INTERNAL_FLASH_FILESYSTEM = 1)
// to make it easier to switch between Arduino and CircuitPython
#define INTERNAL_FLASH_FILESYSTEM_START_ADDR  (0x00040000 - 256 - 0 - INTERNAL_FLASH_FILESYSTEM_SIZE)
#define INTERNAL_FLASH_FILESYSTEM_SIZE        (128*1024) // MAX 255, op XIAO is 192 ook al te veel

// Internal Flash object
Adafruit_InternalFlash flash(INTERNAL_FLASH_FILESYSTEM_START_ADDR, INTERNAL_FLASH_FILESYSTEM_SIZE);

#ifdef LED_BUILTIN
#define LED_PIN LED_BUILTIN
#define LED_ON LOW
#define LED_OFF HIGH
#endif


#endif

// file system object from SdFat
FatVolume fatfs;

FatFile root;
FatFile file;

// USB MSC object
Adafruit_USBD_MSC usb_msc;

// Set to true when PC write to flash
bool fs_changed;

void init_usb_msc()
{

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_callback, msc_write_callback, msc_flush_callback);

  // Set disk size, block size should be 512 regardless of flash page size
  usb_msc.setCapacity(flash.size() / 512, 512);

  // Set Lun ready
  usb_msc.setUnitReady(true);

  usb_msc.begin();

}


// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_callback (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: InternalFlash/SPIFlash Block API: readBlocks/writeBlocks/syncBlocks
  // already include sector caching (if needed). We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_callback (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
#ifdef LED_PIN
  digitalWrite(LED_PIN, LED_ON);
#endif

  // Note: InternalFlash/SPIFlash Block API: readBlocks/writeBlocks/syncBlocks
  // already include sector caching (if needed). We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_callback (void)
{
  // sync with flash
  flash.syncBlocks();

  // clear file system's cache to force refresh
  fatfs.cacheClear();

  fs_changed = true;

#ifdef LED_PIN
  digitalWrite(LED_PIN, LED_OFF);
#endif
}

// the setup function runs once when you press reset or power the board
void setup()
{
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
#endif

  // Initialize internal flash
  flash.begin();

  init_usb_msc();

  // Init file system on the flash (stond oorspronkelijk na usb_msc initialisatie code)
  fatfs.begin(&flash);

  fs_changed = true; // to print contents initially

  Serial.begin(115200);
  Serial.println("Adafruit TinyUSB Mass Storage Flash example");
  Serial.print("Flash size: "); Serial.print(flash.size() / 1024); Serial.println(" KB");
}

void loop()
{
  delay(100);

  if ( fs_changed )
  {
    fs_changed = false;
    Serial.println("fs_changed");
  }

  if (!Serial.available())
    return;

  char c = Serial.read();
  if (c == 'l')
  {
    // List all the files in the flash drive
    Serial.println("Listing files");
    if ( !root.open("/") )
    {
      Serial.println("open root failed");
      return;
    }

    Serial.println("Flash contents:");

    // Open next file in root.
    // Warning, openNext starts at the current directory position
    // so a rewind of the directory may be required.
    while ( file.openNext(&root, O_RDONLY) )
    {
      file.printFileSize(&Serial);
      Serial.write(' ');
      file.printName(&Serial);
      if ( file.isDir() )
      {
        // Indicate a directory.
        Serial.write('/');
      }
      Serial.println();
      file.close();
    }

    root.close();

    Serial.println();
  }
  else if (c == 'w')
  {
    Serial.println("Write a file");
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File myFile = fatfs.open("test.txt", FILE_WRITE);

    // if the file opened okay, write to it:
    if (myFile) {
      Serial.println("Writing to test.txt...");
      myFile.println("testing 1, 2, 3.");
      // close the file:
      myFile.close();
      Serial.println("done.");
    } else {
      // if the file didn't open, print an error:
      Serial.println("error opening test.txt");
    }
  }
}
