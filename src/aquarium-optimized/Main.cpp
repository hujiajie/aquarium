//
// Copyright (c) 2019 The Aquarium Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main.cpp: Entry class of Aquarium.

#include "Aquarium.h"

#if __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static Aquarium aquarium;

void render() {
    aquarium.render();
}

int main(int argc, char **argv) {
    if (!aquarium.init(argc, argv))
    {
        printf("init failed\n");
        return -1;
    }

#if __EMSCRIPTEN__
    emscripten_set_main_loop(render, 0, false);
#else
    aquarium.display();
#endif
}
