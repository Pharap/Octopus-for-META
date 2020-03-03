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
}

// rendering buffers
uint16_t buffer1[screenWidth * sliceHeight];
uint16_t buffer2[screenWidth * sliceHeight];

// flag for an ongoing data transfer
bool drawPending = false;


void drawSprite(Sprite sprite, uint8_t sliceY, uint16_t * buffer) {
  // Precalculate common subexpressions
  uint16_t spriteEnd = (sprite.y + sprite.h);
  uint16_t sliceEnd = (sliceY + sliceHeight);

  // Check if sprite has one part to show on the current slice
  if ((sliceY < spriteEnd) && (sprite.y < sliceEnd)) {
    // Determine the boundaries of the sprite surface within the current slice
    uint8_t xMin = sprite.x;
    uint8_t xmax = ((sprite.x + sprite.w) - 1);
    uint8_t yMin = (sliceY > sprite.y) ? sliceY : sprite.y;
    uint8_t yMax = (sliceEnd <= spriteEnd) ? (sliceEnd - 1) : (spriteEnd - 1);

    // Display the sprite pixels to be drawn
    for (uint8_t py = yMin; py <= yMax; ++py) {
      for (uint8_t px = xMin; px <= xmax; ++px) {
        // Select the spritesheet
        const uint16_t * spritesheet = (sprite.spritesheet == idSpritesheetA) ? spritesheetA : spritesheetB;

        // Pick the pixel color from the spritesheet
        uint16_t color = spritesheet[px + (py * screenWidth)];

        // It colo is different from the transparency color
        if (color != transColor) {
          // Copies the color code into the rendering buffer
          size_t index = (px + ((py - sliceY) * screenWidth));
          buffer[index] = color;
        }
      }
    }
  }
}


void drawText(Sprite sprite, uint8_t sliceY, uint16_t * buffer, uint8_t x, uint8_t y) {
  // Precalculate common subexpressions
  uint16_t spriteEnd = (y + sprite.h);
  uint16_t sliceEnd = (sliceY + sliceHeight);

  if ((sliceY < spriteEnd) && (y < sliceEnd)) {
    uint8_t xMin = x;
    uint8_t xmax = (x + sprite.w - 1);
    uint8_t yMin = (y < sliceY) ? sliceY : y;
    uint8_t yMax = (spriteEnd >= sliceEnd) ? (sliceEnd - 1) : (spriteEnd - 1);

    for (uint8_t py = yMin; py <= yMax; ++py) {
      uint8_t sy = (py - (y + sprite.y));

      for (uint8_t px = xMin; px <= xmax; ++px) {
        uint8_t sx = (px - (xMin + sprite.x));
        uint16_t color = spritesheetA[sx + (sy * screenWidth)];

        if (color != transColor) {
          size_t index = (px + ((py - sliceY) * screenWidth));
          buffer[index] = color;
        }
      }
    }
  }
}


void drawScore(uint16_t displayScore, uint8_t sliceY, uint16_t * buffer) {
  // Check of all that the intersection between
  // the sprite and the current slice is not empty
  if ((sliceY < 20) && ((sliceY + sliceHeight) > 11)) {
    // Determine the boundaries of the sprite surface within the current slice
    uint8_t  xMin = 12;
    uint8_t  yMin = (sliceY > 12) ? sliceY : 12;
    uint8_t  yMax = ((sliceY + sliceHeight) <= 19) ? ((sliceY + sliceHeight) - 1) : 18;
    uint16_t remainder = displayScore;

    // Draw each digit of the score
    // The loop has 4 iterations because 10000 is pow(10, 4)
    for (uint16_t divisor = 10000; divisor > 0; divisor /= 10) {
      // The quotient is the digit to be displayed
      uint16_t quotient = (remainder / divisor);

      // The remainder is what's left over
      remainder = (remainder % divisor);

      // Move the cursor along 6 pixels each time
      xMin += 6;
      
      // Go through the sprite pixels to be drawn
      for (uint8_t py = yMin; py <= yMax; ++py) {
        for (uint8_t px = 1; px <= 5; ++px) {

          // Calculate the colour offset
          size_t colourIndex = (px + (6 * quotient) + ((py - 11) * screenWidth));

          // Pick the pixel colour from the spritesheet
          uint16_t colour = spritesheetA[colourIndex];

          // If the colour is not the transparency colour
          if (colour != transColor) {
            // Calculate the colour offset
            size_t bufferIndex = (xMin + px + ((py - sliceY) * screenWidth));

            // Copy the colour code into the rendering buffer
            buffer[bufferIndex] = colour;
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


void drawBackground(const uint16_t * background, const Sprite spriteToDisplay, uint8_t spriteX, uint8_t spriteY, boolean displaySprite) {
    constexpr size_t bufferSize = (sizeof(uint16_t) * screenWidth * sliceHeight);

    for (uint8_t sliceIndex = 0; sliceIndex < slices; ++sliceIndex)
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
