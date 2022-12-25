// https://github.com/adafruit/Adafruit_TinyUSB_Arduino/issues/144
// https://github.com/adafruit/Adafruit_Learning_System_Guides/blob/main/Adafruit_ESP32S2_TFT_WebServer/Adafruit_ESP32S2_TFT_WebServer.ino

/*
    Board: Lolin S2 Mini
    USB CDC On Boot: enabled
    USB Firmware MSC On Boot: Disabled
    USB DFU On Boot: Disabled
    ==>>Partition Scheme: Default 4MB with ffat (1.2 MB App/1.5MB FATFS)
    Core Debug Level: Geen
    Erase All Flash before Sketch upload: disabled

    ESP32 Arduino core: 2.0.5
    Library
    - SdFat: Adafruit fork 2.2.1
    - Adafruit SPIFlash : 4.0.0
    - TinyUSB: 1.16.0

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

/* This example demo how to expose on-board external Flash as USB Mass Storage.
   Following library is required
     - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
     - SdFat https://github.com/adafruit/SdFat

   Note: Adafruit fork of SdFat enabled ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT
   in SdFatConfig.h, which is needed to run SdFat on external flash. You can use original
   SdFat library and manually change those macros
*/

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

// ARDUINO_ARCH_ESP32
Adafruit_FlashTransport_ESP32 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatFileSystem fatfs;



// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  // sync with flash
  flash.syncBlocks();

  // clear file system's cache to force refresh
  fatfs.cacheClear();

  digitalWrite(LED_BUILTIN, LOW);
}

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  flash.begin();

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.size() / 512, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);

  usb_msc.begin();

  // Init file system on the flash
  fatfs.begin(&flash);

  Serial.begin(115200);
  // while ( !Serial ) delay(10);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mass Storage External Flash example");
  Serial.print("JEDEC ID: 0x"); Serial.println(flash.getJEDECID(), HEX);
  Serial.print("Flash size: "); Serial.print(flash.size() / 1024); Serial.println(" KB");

}

void loop()
{
  delay(100);

  if (!Serial.available())
    return;

  char c = Serial.read();
  if (c == 'm')
  {
    Serial.println("Mount filesystem");
  }
  else if (c == 'l')
  {
    // List all the files in the internal flash drive
    Serial.println("Listing files");
    //    SdFile root;
    FatFile root;


    if (!root.open("/")) {
      Serial.println("open root failed");
    }
    // Open next file in root.
    // Warning, openNext starts at the current directory position
    // so a rewind of the directory may be required.
    //    File file;
    FatFile file;

    while (file.openNext(&root, O_RDONLY))
    {
      file.printFileSize(&Serial);
      Serial.write(' ');
      file.printModifyDateTime(&Serial);
      Serial.write(' ');
      file.printName(&Serial);
      if (file.isDir()) {
        // Indicate a directory.
        Serial.write('/');
      }
      Serial.println();
      // The file should be close to go to the next file
      file.close();
    }
    root.close(); // !!!
  }
  else if (c == 'c')
  {
    Serial.println("Create a file");
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File myFile = fatfs.open("testCreate.txt", FILE_WRITE);

    // if the file opened okay, write to it:
    if (myFile) {
      // close the file:
      myFile.close();
      // sync with flash
      msc_flush_cb(); // ?????
      Serial.println("done.");
    } else {
      // if the file didn't open, print an error:
      Serial.println("error opening testCreate.txt");
    }
  }
  else if (c == 'w')
  {
    Serial.println("Write a file");
    // open the file
    File myFile = fatfs.open("testWrite.txt", FILE_WRITE);

    // if the file opened okay, write to it:
    if (myFile) {
      Serial.print("Writing to testWrite.txt...");
      myFile.println("testing 1, 2, 3.");
      myFile.flush(); // ???
      delay(50); // ???
      // close the file:
      myFile.close();
      // todo sync ??
      Serial.println("done.");
    } else {
      // if the file didn't open, print an error:
      Serial.println("error opening testWrite.txt");
    }
  }
}
