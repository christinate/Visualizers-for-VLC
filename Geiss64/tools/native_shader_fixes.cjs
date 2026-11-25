// SPDX-License-Identifier: Apache-2.0
// Explicit native-only corrections. Preserved upstream sources are never edited.
'use strict';
const calibrationFixId='calibration-final-square-index';
function applyCalibrationFinalSquare(source){
  const original='kHdrHeadroomValues[u32(clamp(square_index, 0.0, 12.0))]';
  const corrected='kHdrHeadroomValues[u32(clamp(square_index, 0.0, f32(kSquareCount - 1u)))]';
  if(!source.includes('const kSquareCount = 14u;')||source.split(original).length!==2)
    throw Error('Expected exactly one original fourteen-square calibration lookup');
  return source.replace(original,corrected);
}
module.exports={calibrationFixId,applyCalibrationFinalSquare};
