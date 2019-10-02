//
// Copyright (c) 2019 The Aquarium Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Aquarium.h"
#include "ContextFactory.h"

#ifdef ENABLE_OPENGL_BACKEND
#include "opengl/ContextGL.h"
#endif
#ifdef ENABLE_DAWN_BACKEND
#include "dawn/ContextDawn.h"
#endif
#ifdef ENABLE_D3D12_BACKEND
#include "d3d12/ContextD3D12.h"
#endif

ContextFactory::ContextFactory() : mContext(nullptr) {}

ContextFactory::~ContextFactory()
{
    delete mContext;
}

Context *ContextFactory::createContext(BACKENDTYPE backendType)
{
    switch (backendType)
    {
        case BACKENDTYPE::BACKENDTYPEOPENGL:
        case BACKENDTYPE::BACKENDTYPEANGLE:
            {
#ifdef ENABLE_OPENGL_BACKEND
                mContext = new ContextGL(backendType);
#endif
                break;
            }
        case BACKENDTYPE::BACKENDTYPEDAWND3D12:
        case BACKENDTYPE::BACKENDTYPEDAWNMETAL:
        case BACKENDTYPE::BACKENDTYPEDAWNVULKAN:
        case BACKENDTYPE::BACKENDTYPEWEBGPU:
            {
#ifdef ENABLE_DAWN_BACKEND
                mContext = new ContextDawn(backendType);
#endif
                break;
            }
            case BACKENDTYPE::BACKENDTYPED3D12:
            {
#ifdef ENABLE_D3D12_BACKEND
                mContext = new ContextD3D12(backendType);
                break;
#endif
            }
            default:
                break;
    }

    return mContext;
}
