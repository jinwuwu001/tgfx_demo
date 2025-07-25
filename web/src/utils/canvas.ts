/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import {CANVAS_POOL_MAX_SIZE} from '../constant';
import {isInstanceOf} from './type-utils';
import {SAFARI_OR_IOS_WEBVIEW, WORKER} from './ua';

const canvasPool = new Array<HTMLCanvasElement | OffscreenCanvas>();

export const isOffscreenCanvas = (element: any) => isInstanceOf(element, globalThis.OffscreenCanvas);

export const isCanvas = (element: any) =>
    isOffscreenCanvas(element) || isInstanceOf(element, globalThis.HTMLCanvasElement);

export const getCanvas2D = (width: number, height: number) => {
    let canvas = canvasPool.pop() || createCanvas2D();
    if (canvas !== null) {
        canvas.width = width;
        canvas.height = height;
    }
    return canvas;
};

export const releaseCanvas2D = (canvas: HTMLCanvasElement | OffscreenCanvas) => {
    if (canvasPool.length < CANVAS_POOL_MAX_SIZE) {
        canvasPool.push(canvas);
    }
};

const createCanvas2D = () => {
    /**
     * Safari browser does not support OffscreenCanvas before version 16.4.
     * After version 16.4, OffscreenCanvas is supported, but type checking errors still exist for WebGL interfaces on OffscreenCanvas.
     * Therefore, HTMLCanvas Element is used uniformly in Safari.
     */
    if (SAFARI_OR_IOS_WEBVIEW && !WORKER) {
        return document.createElement('canvas');
    }
    try {
        const offscreenCanvas = new OffscreenCanvas(0, 0);
        const context = offscreenCanvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
        if (typeof context.measureText === 'function') return offscreenCanvas;
        return document.createElement('canvas');
    } catch (err) {
        return document.createElement('canvas');
    }
};

export const calculateDisplaySize = (canvas: HTMLCanvasElement) => {
    const styleDeclaration = globalThis.getComputedStyle(canvas, null);
    const computedSize = {
        width: Number(styleDeclaration.width.replace('px', '')),
        height: Number(styleDeclaration.height.replace('px', '')),
    };
    if (computedSize.width > 0 && computedSize.height > 0) {
        return computedSize;
    } else {
        const styleSize = {
            width: Number(canvas.style.width.replace('px', '')),
            height: Number(canvas.style.height.replace('px', '')),
        };
        if (styleSize.width > 0 && styleSize.height > 0) {
            return styleSize;
        } else {
            return {
                width: canvas.width,
                height: canvas.height,
            };
        }
    }
};
