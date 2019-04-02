Examples
========

## common.h
Common files for all

# gd.c

Usage: gd [args]

        Where args are:

  -E, --histeq          histogram equalisation
  -H, --hcuthigh=arg    histogram cut-off high limit
  -L, --hcutlow=arg     histogram cut-off low limit
  -T, --transform=arg   type of intensity transformation (exp, lin, log, pow, sqrt)
  -h, --help            show this help
  -i, --inname=arg      name of input file
  -l, --histlvl=arg     amount of levels for histogram calculation
  -n, --hdunumber=arg   open image from given HDU number
  -o, --outpname=arg    output file name (jpeg)
  -p, --palette=arg     convert as given palette (br, cold, gray, hot, jet)
  -r, --rewrite         rewrite output file
  -t, --textline=arg    add text line to output image (at bottom)

## imstat.c

Usage: imstat [args] input files
Get statistics and modify images from first image HDU of each input file
        Where args are:

  -a, --add=arg        add some value (double, or 'mean', 'std', 'min', 'max')
  -h, --help           show this help
  -m, --multiply=arg   multiply by some value (double, operation run after adding)
  -o, --outfile=arg    output file name (collect all input files)
  -z, --rmneg          remove negative values (assign them to 0)


## keylist.c

Usage: keylist [args] infile.fits

        Where args are:

  -a, --addrec         add record to first HDU (you can add more than one record in once, point more -a)
  -c, --contents       show short file contents
  -h, --help           show this help
  -i, --infile=arg     input file name (you can also point it without any keys)
  -l, --list           list all keywords
  -m, --modify         modify values of given keys (each param should be "key = new_value")
  -o, --output=arg     save result to file (else save to same file)

Contains example of protected file writing (blocking all possible signals).

## listtable.c

Usage: listtable [args]

        Where args are:

  -h, --help           show this help
  -i, --fitsname=arg   name of input file
  -l, --list           list all tables in file
  -o, --outfile=arg    output file name

