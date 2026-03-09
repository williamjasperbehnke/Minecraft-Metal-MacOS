#pragma once

#import <Cocoa/Cocoa.h>

namespace mc {
class Minecraft;
}

@interface InventoryView : NSView

- (void)updateFromGame:(const mc::Minecraft&)game;
- (void)showHotbarTooltipForTile:(int)tile;
- (int)slotIndexAtPoint:(NSPoint)viewPoint;
- (int)hoveredSlotIndex;

@end
