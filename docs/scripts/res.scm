;*******************************************************************************
; Ups the resolution of documentation images while keeping the size the same.
;
; Images look fine in Mac Preview and Linux Document Viewer (evince) but don't
; look so good in Adobe Reader which doesn't seem to anti-alias well.
;
; This script (Script-Fu) takes the PNG files exported from Inkscape and uses
; GIMP in batch mode to up the resolution and keep the image size the same.
;
; To run the script, put it in ~/.gimp-2.8/scripts (GIMP version may differ)
; then run the following in the docs/images directory:
;
; $ gimp -i -b '(improve-resolution)' -b '(gimp-quit 0)'
;
;*******************************************************************************
(define (improve-resolution)
  (let*
    (
      (filelist (cadr (file-glob "*.png" 1)))
    )
    (while (not (null? filelist))
      (let*
        (
          (filename (car filelist))
          (image (car (gimp-file-load RUN-NONINTERACTIVE filename filename)))
          (drawable (car (gimp-image-get-active-layer image)))
          (res (gimp-image-get-resolution image))
          (resX (car res))
          (resY (cadr res))
        )
        (gimp-context-set-interpolation INTERPOLATION-NONE)
        (gimp-image-set-resolution image (* 3 resX) (* 3 resY))
        (let*
          (
            (width (car (gimp-image-width image)))
            (height (car (gimp-image-height image)))
          )
          (gimp-context-set-interpolation INTERPOLATION-CUBIC)
          (gimp-image-scale image (* 3 width) (* 3 height))
        )
        (gimp-file-save RUN-NONINTERACTIVE image drawable filename filename)
        (gimp-image-delete image)
      )
      (set! filelist (cdr filelist))
    )
  )
)
