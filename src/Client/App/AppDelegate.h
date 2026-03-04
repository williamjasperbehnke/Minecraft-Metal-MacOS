#pragma once

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

#include <vector>

#include "Client/Core/Minecraft.h"
#include "Client/Render/Metal/MetalRenderer.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, MTKViewDelegate>
@end
