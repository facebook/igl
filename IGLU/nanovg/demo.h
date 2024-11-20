#ifndef DEMO_H
#define DEMO_H

#include "nanovg.h"
#include <string>
#include <functional>

using ImageFullPathCallback = std::function<std::string(const std::string &)>;

void SetImageFullPathCallback(ImageFullPathCallback callback);

#ifdef __cplusplus
extern "C" {
#endif

struct DemoData {
	int fontNormal, fontBold, fontIcons, fontEmoji;
	int images[12];
};
typedef struct DemoData DemoData;

int loadDemoData(NVGcontext* vg, DemoData* data);
void freeDemoData(NVGcontext* vg, DemoData* data);
void renderDemo(NVGcontext* vg, float mx, float my, float width, float height, float t, int blowup, DemoData* data);

void saveScreenShot(int w, int h, int premult, const char* name);

#ifdef __cplusplus
}
#endif

#endif // DEMO_H
