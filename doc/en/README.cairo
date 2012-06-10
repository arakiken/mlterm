comment -*- mode: text -*-
comment $Id$

Note for cairo support.

* Installation
  $ ./configure --type-engines=cairo(,xft,xcore)
  $ make
  $ make install

* Usage
  $ mlterm --type cairo

* Notice
  ":PERCENT" specified in ~/.mlterm/*font is ignored for now.
