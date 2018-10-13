-- sample adapted from https://www.mpfr.org/sample.html

local mpfr = require('mpfr')

mpfr.set_default_prec(200)
mpfr.set_default_rounding_mode(mpfr.RNDD)

local s, t, u = mpfr.new(), mpfr.new(), mpfr.new()

s: set(1)
t: set(1)

for i = 1, 100 do
	u: div(u:set(1), t:mul(t, i, mpfr.RNDU))
	s: add(s, u)
end

print("sum is", s)
