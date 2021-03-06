#ifndef HD_TOOLS
#define HD_TOOLS

#include "Global.h"
#include "Sprites.h"
#include "Pictures.h"

// -------------------------------------------------------------------------
// Initialization related to the DMA controller
// -------------------------------------------------------------------------

namespace Gamebuino_Meta {

#define DMA_DESC_COUNT 3
extern volatile uint32_t dma_desc_free_count;

static inline void wait_for_transfers_done(void) {
  while (dma_desc_free_count < DMA_DESC_COUNT);
}
static SPISettings tftSPISettings = SPISettings(24000000, MSBFIRST, SPI_MODE0);
};

// rendering buffers
uint16_t buffer1[screenWidth * sliceHeight];
uint16_t buffer2[screenWidth * sliceHeight];

// flag for an ongoing data transfer
bool drawPending = false;


void drawSprite(Sprite sprite, uint8_t sliceY, uint16_t* buffer) {
  // Check if sprite has one part to show on the current slice
  if (sliceY < sprite.y + sprite.h && sprite.y < sliceY + sliceHeight) {
    // Determines the boundaries of the sprite surface within the current slice
    uint8_t  xMin = sprite.x;
    uint8_t  xmax = sprite.x + sprite.w - 1;
    uint8_t  yMin = sprite.y < sliceY ? sliceY : sprite.y;
    uint8_t  yMax = sprite.y + sprite.h >= sliceY + sliceHeight ? sliceY + sliceHeight - 1 : sprite.y + sprite.h - 1;
    uint8_t  px, py;
    uint16_t color;
    // Display the sprite pixels to be drawn
    for (py = yMin; py <= yMax; py++) {
      for (px = xMin; px <= xmax; px++) {
        // Picks the pixel color from the spritesheet
        color = spritesheet[px + py * screenWidth];
        // If color is the color selected for the Sprite
        if (color == sprite.color) {
          // Copies the color code into the rendering buffer
          buffer[px + (py - sliceY) * screenWidth] = sBlack;
        }
      }
    }
  }
}


void drawText(Sprite sprite, uint8_t sliceY, uint16_t* buffer, uint8_t x, uint8_t y) {
  if (sliceY < y + sprite.h && y < sliceY + sliceHeight) {
    uint8_t xMin = x;
    uint8_t xmax = x + sprite.w - 1;
    uint8_t yMin = y < sliceY ? sliceY : y;
    uint8_t yMax = y + sprite.h >= sliceY + sliceHeight ? sliceY + sliceHeight - 1 : y + sprite.h - 1;

    uint8_t  px, py, sx, sy;
    uint16_t color;

    for (py = yMin; py <= yMax; ++py) {
      sy = py - y + sprite.y;
      for (px = xMin; px <= xmax; ++px) {
        sx = px - xMin + sprite.x;
        color = spritesheet[sx + sy * screenWidth];
        if (color == sRed) {
          buffer[px + (py - sliceY) * screenWidth] = sBlack;
        }
      }
    }
  }
}


void drawScore(uint16_t displayScore, uint8_t sliceY, uint16_t* buffer) {
  // we check first of all that the intersection between
  // the sprite and the current slice is not empty
  if (sliceY < 20 && sliceY + sliceHeight > 11 ) {
    // determines the boundaries of the sprite surface within the current slice
    uint8_t  xMin = 102;
    uint8_t  yMin = (12 < sliceY) ? sliceY : 12;
    uint8_t  yMax = (19 >= sliceY + sliceHeight) ? sliceY + sliceHeight - 1 : 18;
    uint16_t color;
    uint16_t remainder = displayScore;

    // Draw each digit of the score
    for (uint16_t divisor = 1000; divisor > 0; divisor /= 10)     {  // The limit is 4 because 10000 is pow(10, 4)
      uint16_t quotient = (remainder / divisor);
      remainder = (remainder % divisor);
      xMin += 6;
      // Go through the sprite pixels to be drawn
      for (uint8_t py = yMin; py <= yMax; ++py)   {
        for (uint8_t px = 0; px < 4; ++px)   {

          // Calculate the colour offset
          size_t colourIndex = (px + (6 * quotient) + ((py - 8) * screenWidth));

          // Pick the pixel colour from the spritesheet
          uint16_t colour = spritesheet[colourIndex];

          // If it is not the transparency colour
          if (colour == sRed)     {
            // Calculate the colour offset
            size_t bufferIndex = (xMin + px + ((py - sliceY) * screenWidth));
            // Copy the colour code into the rendering buffer
            buffer[bufferIndex] = sBlack ;
          }
        }
      }
    }
  }
}


// -------------------------------------------------------------------------
// Memory transfer to DMA controller
// -------------------------------------------------------------------------

// initiates memory forwarding to the DMA controller....
void customDrawBuffer(uint8_t x, uint8_t y, uint16_t* buffer, uint8_t w, uint8_t h) {
  drawPending = true;
  gb.tft.setAddrWindow(x, y, x + w - 1, y + h - 1);
  SPI.beginTransaction(Gamebuino_Meta::tftSPISettings);
  gb.tft.dataMode();
  gb.tft.sendBuffer(buffer, w * h);
}

// waits for the memory transfer to be completed
// and close the transaction with the DMA controller
void waitForPreviousDraw() {
  if (drawPending) {
    Gamebuino_Meta::wait_for_transfers_done();
    gb.tft.idleMode();
    SPI.endTransaction();
    drawPending = false;
  }
}


void drawBackground(const uint16_t * background, const Sprite spriteToDisplay, uint8_t spriteX, uint8_t spriteY, boolean displaySprite)
{
  constexpr size_t bufferSize = (sizeof(uint16_t) * screenWidth * sliceHeight);

  for (uint8_t sliceIndex = 0; sliceIndex < slices; sliceIndex++)
  {
    // buffers are switched according to the parity of sliceIndex
    uint16_t * buffer = (sliceIndex % 2 == 0) ? buffer1 : buffer2;

    // the top border of the current slice is calculated
    uint8_t sliceY = sliceIndex * sliceHeight;

    size_t backgroundOffset = (sliceY * screenWidth);

    // starts by drawing the background
    memcpy(buffer, &background[backgroundOffset], bufferSize);

    // and finally draws the sprite if needed
    if (displaySprite == true) drawText(spriteToDisplay, sliceY, buffer, spriteX, spriteY);

    // then we make sure that the sending of the previous buffer
    // to the DMA controller has taken place
    if (sliceIndex != 0)
      waitForPreviousDraw();

    // after which we can then send the current buffer
    customDrawBuffer(0, sliceY, buffer, screenWidth, sliceHeight);
  }

  // always wait until the DMA transfer is completed
  // for the last slice before entering the next cycle
  waitForPreviousDraw();
}

#endif
