#import "Client/App/CrosshairView.h"

@implementation CrosshairView

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self setWantsLayer:YES];
    self.layer.backgroundColor = NSColor.clearColor.CGColor;
  }
  return self;
}

- (BOOL)isOpaque {
  return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  [[NSColor colorWithWhite:1.0 alpha:0.95] setStroke];

  NSBezierPath* path = [NSBezierPath bezierPath];
  path.lineWidth = 2.0;

  const NSRect b = self.bounds;
  const CGFloat cx = NSMidX(b);
  const CGFloat cy = NSMidY(b);
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
