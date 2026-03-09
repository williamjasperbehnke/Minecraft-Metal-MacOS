#import "Client/App/CrosshairView.h"
#import "Client/App/UiImageHelpers.h"

#include <algorithm>

@implementation CrosshairView
{
  NSImage* _iconsImage;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self setWantsLayer:YES];
    self.layer.backgroundColor = NSColor.clearColor.CGColor;
    NSString* iconsPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/icons.png");
    if (iconsPath) {
      _iconsImage = [[NSImage alloc] initWithContentsOfFile:iconsPath];
    }
  }
  return self;
}

- (BOOL)isOpaque {
  return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  const NSRect b = self.bounds;
  const CGFloat cx = NSMidX(b);
  const CGFloat cy = NSMidY(b);
  const CGFloat size = 16.0;
  const NSRect dst = NSMakeRect(std::floor(cx - size * 0.5), std::floor(cy - size * 0.5), size, size);

  if (_iconsImage) {
    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
    const NSRect src = mc::app::ui::atlasSrcRectTopLeftPixels(_iconsImage, 3.0, 3.0, 9.0, 9.0);
    [_iconsImage drawInRect:dst fromRect:src operation:NSCompositingOperationSourceOver fraction:1.0];
    return;
  }

  [[NSColor colorWithWhite:1.0 alpha:0.95] setStroke];
  NSBezierPath* path = [NSBezierPath bezierPath];
  path.lineWidth = 2.0;
  const CGFloat arm = 8.0;
  const CGFloat gap = 3.0;
  [path moveToPoint:NSMakePoint(cx - arm, cy)];
  [path lineToPoint:NSMakePoint(cx - gap, cy)];
  [path moveToPoint:NSMakePoint(cx + gap, cy)];
  [path lineToPoint:NSMakePoint(cx + arm, cy)];
  [path moveToPoint:NSMakePoint(cx, cy - arm)];
  [path lineToPoint:NSMakePoint(cx, cy - gap)];
  [path moveToPoint:NSMakePoint(cx, cy + gap)];
  [path lineToPoint:NSMakePoint(cx, cy + arm)];
  [path stroke];
}

@end
