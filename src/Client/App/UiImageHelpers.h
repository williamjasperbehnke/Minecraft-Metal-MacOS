#pragma once

#import <Cocoa/Cocoa.h>

namespace mc::app::ui {

inline NSString* findAssetPath(NSString* relativePath) {
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

inline CGSize imagePixelSize(NSImage* image) {
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

inline NSRect atlasSrcRectTopLeftPixels(NSImage* image, CGFloat pxX, CGFloat pxY, CGFloat pxW, CGFloat pxH) {
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

}  // namespace mc::app::ui
