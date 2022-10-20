# Orthorectification Tool
![image](https://user-images.githubusercontent.com/1951843/111536715-fc91c380-8740-11eb-844c-5b7960186391.png)

This tool is capable of orthorectifying individual images (or all images) from an existing ODM reconstruction.

![image](https://user-images.githubusercontent.com/1951843/111529183-3ad6b500-8738-11eb-9960-b1aa676f863b.png)

## Building

Use the build scripts `build_release` and `build_debug` to compile natively.

Otherwise you can build the docker image:
```
docker build -t digipa/orthorectify .
```

## Usage
After running a reconstruction using ODM:

```
Orthorectify /dataset-path
```
With docker

```
docker run -it -v /dataset-path:/dataset digipa/orthorectify /dataset
```
This will start the orthorectification process for all images in the dataset. See additional flags you can pass at the end of the command above:
```
This tool is capable of orthorectifying individual images (or all images) from an existing ODM reconstruction.
Usage:
  Orthorectify [OPTION...] positional parameters

  -e, --dem arg               Absolute path to DEM to use to orthorectify
                              images (default: odm_dem/dsm.tif)
      --no-alpha              Don't output an alpha channel
  -i, --interpolation arg     Type of interpolation to use to sample pixel
                              values (nearest, bilinear) (default:
                              bilinear)
  -o, --outdir arg            Output directory where to store results
                              (default: orthorectified)
  -l, --image-list arg        Path to file that contains the list of image
                              filenames to orthorectify. By default all
                              images in a dataset are processed (default:
                              img_list.txt)
      --images arg            Comma-separated list of filenames to rectify.
                              Use as an alternative to --image-list
  -s, --skip-visibility-test  Skip visibility testing (faster but leaves
                              artifacts due to relief displacement)
  -t, --threads arg           Number of threads to use (-1 = all) (default:
                              -1)
  -v, --verbose               Verbose logging
  -h, --help                  Print usage
```
-----------------
## Roadmap
Help us improve this module! We could add:
- [ ] Merging of multiple orthorectified images (blending, filtering, seam leveling)
- [ ] Faster visibility test
- [ ] Different methods for orthorectification (direct)
- [ ] GPU Support