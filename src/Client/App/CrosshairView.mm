#import "Client/App/CrosshairView.h"

#include <algorithm>

@implementation CrosshairView
{
  NSImage* _iconsImage;
}

static NSString* findAssetPath(NSString* relativePath) {
  NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
  NSString* cursor = [bundlePath stringByDeletingLastPathComponent];
  NSFileManager* fm = [NSFileManager defaultManager];
  for (int i = 0; i < 10; ++i) {
    NSString* localAssetPath = [cursor stringByAppendingPathComponent:relativePath];
    if ([fm fileExistsAtPath:localAssetPath]) {
      return localAssetPath;
    }
    NSString* parent = [cursor stringByDeletingLastPathComponent];
    if ([parent isEqualToString:cursor]) {
      break;
    }
    cursor = parent;
  }
  return nil;
}

static CGSize imagePixelSize(NSImage* image) {
  if (!image) {
    return CGSizeZero;
  }
  CGImageRef cg = [image CGImageForProposedRect:nullptr context:nil hints:nil];
  if (cg) {
    return CGSizeMake(static_cast<CGFloat>(CGImageGetWidth(cg)), static_cast<CGFloat>(CGImageGetHeight(cg)));
  }
  for (NSImageRep* rep in image.representations) {
    if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
      return CGSizeMake(static_cast<CGFloat>(((NSBitmapImageRep*)rep).pixelsWide),
                        static_cast<CGFloat>(((NSBitmapImageRep*)rep).pixelsHigh));
    }
  }
  return CGSizeMake(image.size.width, image.size.height);
}

static NSRect atlasSrcRectTopLeftPixels(NSImage* image, CGFloat pxX, CGFloat pxY, CGFloat pxW, CGFloat pxH) {
  if (!image) {
    return NSZeroRect;
  }
  const CGSize pixel = imagePixelSize(image);
  if (pixel.width <= 0.0 || pixel.height <= 0.0 || image.size.width <= 0.0 || image.size.height <= 0.0) {
    return NSZeroRect;
  }
  const CGFloat sx = image.size.width / pixel.width;
  const CGFloat sy = image.size.height / pixel.height;
  const CGFloat x = pxX * sx;
  const CGFloat y = image.size.height - ((pxY + pxH) * sy);
  const CGFloat w = pxW * sx;
  const CGFloat h = pxH * sy;
  return NSMakeRect(x, y, w, h);
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self setWantsLayer:YES];
    self.layer.backgroundColor = NSColor.clearColor.CGColor;
    NSString* iconsPath = findAssetPath(@"MinecraftMetal/Assets/icons.png");
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
    const NSRect src = atlasSrcRectTopLeftPixels(_iconsImage, 3.0, 3.0, 9.0, 9.0);
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
