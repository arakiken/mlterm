comment -*- mode: text -*-
comment $Id$

Note for cairo support.

* Installation
  $ ./configure --type-engines=cairo(,xft,xcore)
  $ make
  $ make install

* Usage
  $ mlterm --type cairo

* Not supported features
  o letter space (letter_space option)
  o anti-alias (use_anti_alias option) (partially works)
  o drawing indic characters (use_ind option)
