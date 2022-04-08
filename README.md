# Riemersma-Dither

## Implementaion of riemersma dithering algorithm using parallel processing.

Riemersma dither is a new dithering method that may reduce a greyscale or color image to any color map (also known as a palette) while limiting the dithered pixel's effect to a limited area surrounding it. It therefore combines the benefits of both ordered and error diffusion dither. Image processing systems that employ quadtrees and animation file formats that allow palette files would both benefit from the new approach (especially if those animation file formats use some kind of delta-frame encoding).


The ordered dither algorithm compares a pixel's gray level to a dither matrix level. The pixel is either black or white depending on the result of the comparison. The threshold values in the matrix range from 0 to 255. The dither process is heavily influenced by the size of the matrix and the placement of the values. When a picture is bigger than the dither matrix (which is generally the case), the dither matrix is tiled over it. Only one level of the dither matrix is compared to each pixel in the original picture. The dither output is unaffected by neighboring pixels. The book "Digital Halftoning" delves into the specifics of matrix construction.
