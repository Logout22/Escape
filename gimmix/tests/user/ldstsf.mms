%
% ldstsf.mms -- tests ldsf and stsf (generated)
%
		LOC		#1000

Main	OR		$0,$0,$0	% dummy
		PUT		rA,0

		% Put floats in registers
		SETH	$0,#0000
		ORMH	$0,#0000
		ORML	$0,#0000
		ORL		$0,#0000

		SETH	$1,#8000
		ORMH	$1,#0000
		ORML	$1,#0000
		ORL		$1,#0000

		SETH	$2,#3FF0
		ORMH	$2,#0000
		ORML	$2,#0000
		ORL		$2,#0000

		SETH	$3,#4000
		ORMH	$3,#0000
		ORML	$3,#0000
		ORL		$3,#0000

		SETH	$4,#BFF8
		ORMH	$4,#0000
		ORML	$4,#0000
		ORL		$4,#0000

		SETH	$5,#BFA0
		ORMH	$5,#F57F
		ORML	$5,#737D
		ORL		$5,#A61E

		SETH	$6,#3FF1
		ORMH	$6,#C28F
		ORML	$6,#5C28
		ORL		$6,#F5C3

		SETH	$7,#3FFF
		ORMH	$7,#FFFE
		ORML	$7,#F390
		ORL		$7,#85F5

		SETH	$8,#432F
		ORMH	$8,#FFFF
		ORML	$8,#FFFF
		ORL		$8,#FFFE

		SETH	$9,#BFE8
		ORMH	$9,#0000
		ORML	$9,#0000
		ORL		$9,#0000

		SETH	$10,#C029
		ORMH	$10,#84C5
		ORML	$10,#974E
		ORL		$10,#65BF

		SETH	$11,#4088
		ORMH	$11,#48EA
		ORML	$11,#6D26
		ORL		$11,#7408

		SETH	$12,#369F
		ORMH	$12,#F868
		ORML	$12,#BF4D
		ORL		$12,#956A

		SETH	$13,#369B
		ORMH	$13,#6735
		ORML	$13,#3642
		ORL		$13,#8011

		SETH	$14,#37BA
		ORMH	$14,#2239
		ORML	$14,#3B33
		ORL		$14,#036B

		SETH	$15,#3FFF
		ORMH	$15,#FFFF
		ORML	$15,#FFFF
		ORL		$15,#FFFF

		SETH	$16,#7FEF
		ORMH	$16,#FFFF
		ORML	$16,#FFFF
		ORL		$16,#FFFF

		SETH	$17,#FFEF
		ORMH	$17,#FFFF
		ORML	$17,#FFFF
		ORL		$17,#FFFF

		SETH	$18,#0000
		ORMH	$18,#0000
		ORML	$18,#0000
		ORL		$18,#0001

		SETH	$19,#8000
		ORMH	$19,#0000
		ORML	$19,#0000
		ORL		$19,#0001

		SETH	$20,#7FF0
		ORMH	$20,#0000
		ORML	$20,#0000
		ORL		$20,#0000

		SETH	$21,#FFF0
		ORMH	$21,#0000
		ORML	$21,#0000
		ORL		$21,#0000

		SETH	$22,#7FF8
		ORMH	$22,#0000
		ORML	$22,#0000
		ORL		$22,#0000

		SETH	$23,#7FF4
		ORMH	$23,#0000
		ORML	$23,#0000
		ORL		$23,#0000

		% Setup location for results
		SETL	$24,#E000
		SETL	$25,#F000

		% Perform tests
		% Set rounding mode to NEAR
		SETML	$26,#0
		PUT		rA,$26

		STSF	$0,$24,0		% 0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 0.000000e+00
		STOU	$27,$25,0		% #F000
		ADDU	$25,$25,8

		STSF	$1,$24,0		% -0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -0.000000e+00
		STOU	$27,$25,0		% #F008
		ADDU	$25,$25,8

		STSF	$2,$24,0		% 1.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.000000e+00
		STOU	$27,$25,0		% #F010
		ADDU	$25,$25,8

		STSF	$3,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F018
		ADDU	$25,$25,8

		STSF	$4,$24,0		% -1.500000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.500000e+00
		STOU	$27,$25,0		% #F020
		ADDU	$25,$25,8

		STSF	$5,$24,0		% -3.312300e-02 -> #E000
		LDSF	$27,$24,0		% $27 <- -3.312300e-02
		STOU	$27,$25,0		% #F028
		ADDU	$25,$25,8

		STSF	$6,$24,0		% 1.110000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.110000e+00
		STOU	$27,$25,0		% #F030
		ADDU	$25,$25,8

		STSF	$7,$24,0		% 1.999999e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.999999e+00
		STOU	$27,$25,0		% #F038
		ADDU	$25,$25,8

		STSF	$8,$24,0		% 4.503600e+15 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.503600e+15
		STOU	$27,$25,0		% #F040
		ADDU	$25,$25,8

		STSF	$9,$24,0		% -7.500000e-01 -> #E000
		LDSF	$27,$24,0		% $27 <- -7.500000e-01
		STOU	$27,$25,0		% #F048
		ADDU	$25,$25,8

		STSF	$10,$24,0		% -1.275932e+01 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.275932e+01
		STOU	$27,$25,0		% #F050
		ADDU	$25,$25,8

		STSF	$11,$24,0		% 7.771145e+02 -> #E000
		LDSF	$27,$24,0		% $27 <- 7.771145e+02
		STOU	$27,$25,0		% #F058
		ADDU	$25,$25,8

		STSF	$12,$24,0		% 1.400000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.400000e-45
		STOU	$27,$25,0		% #F060
		ADDU	$25,$25,8

		STSF	$13,$24,0		% 1.200000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.200000e-45
		STOU	$27,$25,0		% #F068
		ADDU	$25,$25,8

		STSF	$14,$24,0		% 3.000000e-40 -> #E000
		LDSF	$27,$24,0		% $27 <- 3.000000e-40
		STOU	$27,$25,0		% #F070
		ADDU	$25,$25,8

		STSF	$15,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F078
		ADDU	$25,$25,8

		STSF	$16,$24,0		% 1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.797693e+308
		STOU	$27,$25,0		% #F080
		ADDU	$25,$25,8

		STSF	$17,$24,0		% -1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.797693e+308
		STOU	$27,$25,0		% #F088
		ADDU	$25,$25,8

		STSF	$18,$24,0		% 4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.940656e-324
		STOU	$27,$25,0		% #F090
		ADDU	$25,$25,8

		STSF	$19,$24,0		% -4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- -4.940656e-324
		STOU	$27,$25,0		% #F098
		ADDU	$25,$25,8

		STSF	$20,$24,0		% inf -> #E000
		LDSF	$27,$24,0		% $27 <- inf
		STOU	$27,$25,0		% #F0A0
		ADDU	$25,$25,8

		STSF	$21,$24,0		% -inf -> #E000
		LDSF	$27,$24,0		% $27 <- -inf
		STOU	$27,$25,0		% #F0A8
		ADDU	$25,$25,8

		STSF	$22,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F0B0
		ADDU	$25,$25,8

		STSF	$23,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F0B8
		ADDU	$25,$25,8

		% Set rounding mode to DOWN
		SETML	$26,#3
		PUT		rA,$26

		STSF	$0,$24,0		% 0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 0.000000e+00
		STOU	$27,$25,0		% #F0C0
		ADDU	$25,$25,8

		STSF	$1,$24,0		% -0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -0.000000e+00
		STOU	$27,$25,0		% #F0C8
		ADDU	$25,$25,8

		STSF	$2,$24,0		% 1.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.000000e+00
		STOU	$27,$25,0		% #F0D0
		ADDU	$25,$25,8

		STSF	$3,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F0D8
		ADDU	$25,$25,8

		STSF	$4,$24,0		% -1.500000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.500000e+00
		STOU	$27,$25,0		% #F0E0
		ADDU	$25,$25,8

		STSF	$5,$24,0		% -3.312300e-02 -> #E000
		LDSF	$27,$24,0		% $27 <- -3.312300e-02
		STOU	$27,$25,0		% #F0E8
		ADDU	$25,$25,8

		STSF	$6,$24,0		% 1.110000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.110000e+00
		STOU	$27,$25,0		% #F0F0
		ADDU	$25,$25,8

		STSF	$7,$24,0		% 1.999999e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.999999e+00
		STOU	$27,$25,0		% #F0F8
		ADDU	$25,$25,8

		STSF	$8,$24,0		% 4.503600e+15 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.503600e+15
		STOU	$27,$25,0		% #F100
		ADDU	$25,$25,8

		STSF	$9,$24,0		% -7.500000e-01 -> #E000
		LDSF	$27,$24,0		% $27 <- -7.500000e-01
		STOU	$27,$25,0		% #F108
		ADDU	$25,$25,8

		STSF	$10,$24,0		% -1.275932e+01 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.275932e+01
		STOU	$27,$25,0		% #F110
		ADDU	$25,$25,8

		STSF	$11,$24,0		% 7.771145e+02 -> #E000
		LDSF	$27,$24,0		% $27 <- 7.771145e+02
		STOU	$27,$25,0		% #F118
		ADDU	$25,$25,8

		STSF	$12,$24,0		% 1.400000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.400000e-45
		STOU	$27,$25,0		% #F120
		ADDU	$25,$25,8

		STSF	$13,$24,0		% 1.200000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.200000e-45
		STOU	$27,$25,0		% #F128
		ADDU	$25,$25,8

		STSF	$14,$24,0		% 3.000000e-40 -> #E000
		LDSF	$27,$24,0		% $27 <- 3.000000e-40
		STOU	$27,$25,0		% #F130
		ADDU	$25,$25,8

		STSF	$15,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F138
		ADDU	$25,$25,8

		STSF	$16,$24,0		% 1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.797693e+308
		STOU	$27,$25,0		% #F140
		ADDU	$25,$25,8

		STSF	$17,$24,0		% -1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.797693e+308
		STOU	$27,$25,0		% #F148
		ADDU	$25,$25,8

		STSF	$18,$24,0		% 4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.940656e-324
		STOU	$27,$25,0		% #F150
		ADDU	$25,$25,8

		STSF	$19,$24,0		% -4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- -4.940656e-324
		STOU	$27,$25,0		% #F158
		ADDU	$25,$25,8

		STSF	$20,$24,0		% inf -> #E000
		LDSF	$27,$24,0		% $27 <- inf
		STOU	$27,$25,0		% #F160
		ADDU	$25,$25,8

		STSF	$21,$24,0		% -inf -> #E000
		LDSF	$27,$24,0		% $27 <- -inf
		STOU	$27,$25,0		% #F168
		ADDU	$25,$25,8

		STSF	$22,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F170
		ADDU	$25,$25,8

		STSF	$23,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F178
		ADDU	$25,$25,8

		% Set rounding mode to UP
		SETML	$26,#2
		PUT		rA,$26

		STSF	$0,$24,0		% 0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 0.000000e+00
		STOU	$27,$25,0		% #F180
		ADDU	$25,$25,8

		STSF	$1,$24,0		% -0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -0.000000e+00
		STOU	$27,$25,0		% #F188
		ADDU	$25,$25,8

		STSF	$2,$24,0		% 1.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.000000e+00
		STOU	$27,$25,0		% #F190
		ADDU	$25,$25,8

		STSF	$3,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F198
		ADDU	$25,$25,8

		STSF	$4,$24,0		% -1.500000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.500000e+00
		STOU	$27,$25,0		% #F1A0
		ADDU	$25,$25,8

		STSF	$5,$24,0		% -3.312300e-02 -> #E000
		LDSF	$27,$24,0		% $27 <- -3.312300e-02
		STOU	$27,$25,0		% #F1A8
		ADDU	$25,$25,8

		STSF	$6,$24,0		% 1.110000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.110000e+00
		STOU	$27,$25,0		% #F1B0
		ADDU	$25,$25,8

		STSF	$7,$24,0		% 1.999999e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.999999e+00
		STOU	$27,$25,0		% #F1B8
		ADDU	$25,$25,8

		STSF	$8,$24,0		% 4.503600e+15 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.503600e+15
		STOU	$27,$25,0		% #F1C0
		ADDU	$25,$25,8

		STSF	$9,$24,0		% -7.500000e-01 -> #E000
		LDSF	$27,$24,0		% $27 <- -7.500000e-01
		STOU	$27,$25,0		% #F1C8
		ADDU	$25,$25,8

		STSF	$10,$24,0		% -1.275932e+01 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.275932e+01
		STOU	$27,$25,0		% #F1D0
		ADDU	$25,$25,8

		STSF	$11,$24,0		% 7.771145e+02 -> #E000
		LDSF	$27,$24,0		% $27 <- 7.771145e+02
		STOU	$27,$25,0		% #F1D8
		ADDU	$25,$25,8

		STSF	$12,$24,0		% 1.400000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.400000e-45
		STOU	$27,$25,0		% #F1E0
		ADDU	$25,$25,8

		STSF	$13,$24,0		% 1.200000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.200000e-45
		STOU	$27,$25,0		% #F1E8
		ADDU	$25,$25,8

		STSF	$14,$24,0		% 3.000000e-40 -> #E000
		LDSF	$27,$24,0		% $27 <- 3.000000e-40
		STOU	$27,$25,0		% #F1F0
		ADDU	$25,$25,8

		STSF	$15,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F1F8
		ADDU	$25,$25,8

		STSF	$16,$24,0		% 1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.797693e+308
		STOU	$27,$25,0		% #F200
		ADDU	$25,$25,8

		STSF	$17,$24,0		% -1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.797693e+308
		STOU	$27,$25,0		% #F208
		ADDU	$25,$25,8

		STSF	$18,$24,0		% 4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.940656e-324
		STOU	$27,$25,0		% #F210
		ADDU	$25,$25,8

		STSF	$19,$24,0		% -4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- -4.940656e-324
		STOU	$27,$25,0		% #F218
		ADDU	$25,$25,8

		STSF	$20,$24,0		% inf -> #E000
		LDSF	$27,$24,0		% $27 <- inf
		STOU	$27,$25,0		% #F220
		ADDU	$25,$25,8

		STSF	$21,$24,0		% -inf -> #E000
		LDSF	$27,$24,0		% $27 <- -inf
		STOU	$27,$25,0		% #F228
		ADDU	$25,$25,8

		STSF	$22,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F230
		ADDU	$25,$25,8

		STSF	$23,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F238
		ADDU	$25,$25,8

		% Set rounding mode to ZERO
		SETML	$26,#1
		PUT		rA,$26

		STSF	$0,$24,0		% 0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 0.000000e+00
		STOU	$27,$25,0		% #F240
		ADDU	$25,$25,8

		STSF	$1,$24,0		% -0.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -0.000000e+00
		STOU	$27,$25,0		% #F248
		ADDU	$25,$25,8

		STSF	$2,$24,0		% 1.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.000000e+00
		STOU	$27,$25,0		% #F250
		ADDU	$25,$25,8

		STSF	$3,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F258
		ADDU	$25,$25,8

		STSF	$4,$24,0		% -1.500000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.500000e+00
		STOU	$27,$25,0		% #F260
		ADDU	$25,$25,8

		STSF	$5,$24,0		% -3.312300e-02 -> #E000
		LDSF	$27,$24,0		% $27 <- -3.312300e-02
		STOU	$27,$25,0		% #F268
		ADDU	$25,$25,8

		STSF	$6,$24,0		% 1.110000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.110000e+00
		STOU	$27,$25,0		% #F270
		ADDU	$25,$25,8

		STSF	$7,$24,0		% 1.999999e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.999999e+00
		STOU	$27,$25,0		% #F278
		ADDU	$25,$25,8

		STSF	$8,$24,0		% 4.503600e+15 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.503600e+15
		STOU	$27,$25,0		% #F280
		ADDU	$25,$25,8

		STSF	$9,$24,0		% -7.500000e-01 -> #E000
		LDSF	$27,$24,0		% $27 <- -7.500000e-01
		STOU	$27,$25,0		% #F288
		ADDU	$25,$25,8

		STSF	$10,$24,0		% -1.275932e+01 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.275932e+01
		STOU	$27,$25,0		% #F290
		ADDU	$25,$25,8

		STSF	$11,$24,0		% 7.771145e+02 -> #E000
		LDSF	$27,$24,0		% $27 <- 7.771145e+02
		STOU	$27,$25,0		% #F298
		ADDU	$25,$25,8

		STSF	$12,$24,0		% 1.400000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.400000e-45
		STOU	$27,$25,0		% #F2A0
		ADDU	$25,$25,8

		STSF	$13,$24,0		% 1.200000e-45 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.200000e-45
		STOU	$27,$25,0		% #F2A8
		ADDU	$25,$25,8

		STSF	$14,$24,0		% 3.000000e-40 -> #E000
		LDSF	$27,$24,0		% $27 <- 3.000000e-40
		STOU	$27,$25,0		% #F2B0
		ADDU	$25,$25,8

		STSF	$15,$24,0		% 2.000000e+00 -> #E000
		LDSF	$27,$24,0		% $27 <- 2.000000e+00
		STOU	$27,$25,0		% #F2B8
		ADDU	$25,$25,8

		STSF	$16,$24,0		% 1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- 1.797693e+308
		STOU	$27,$25,0		% #F2C0
		ADDU	$25,$25,8

		STSF	$17,$24,0		% -1.797693e+308 -> #E000
		LDSF	$27,$24,0		% $27 <- -1.797693e+308
		STOU	$27,$25,0		% #F2C8
		ADDU	$25,$25,8

		STSF	$18,$24,0		% 4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- 4.940656e-324
		STOU	$27,$25,0		% #F2D0
		ADDU	$25,$25,8

		STSF	$19,$24,0		% -4.940656e-324 -> #E000
		LDSF	$27,$24,0		% $27 <- -4.940656e-324
		STOU	$27,$25,0		% #F2D8
		ADDU	$25,$25,8

		STSF	$20,$24,0		% inf -> #E000
		LDSF	$27,$24,0		% $27 <- inf
		STOU	$27,$25,0		% #F2E0
		ADDU	$25,$25,8

		STSF	$21,$24,0		% -inf -> #E000
		LDSF	$27,$24,0		% $27 <- -inf
		STOU	$27,$25,0		% #F2E8
		ADDU	$25,$25,8

		STSF	$22,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F2F0
		ADDU	$25,$25,8

		STSF	$23,$24,0		% nan -> #E000
		LDSF	$27,$24,0		% $27 <- nan
		STOU	$27,$25,0		% #F2F8
		ADDU	$25,$25,8

		% Sync memory
		SETL	$25,#F000
		SYNCD	#FE,$25
		ADDU	$25,$25,#FF
		SYNCD	#FE,$25
		ADDU	$25,$25,#FF
		SYNCD	#FE,$25
		ADDU	$25,$25,#FF
		SYNCD	#6,$25
		ADDU	$25,$25,#7