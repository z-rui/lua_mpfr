local mpfr = require 'mpfr'

-- compute the first N digits in π.
local N=1000
local prec=math.ceil(math.log(10,2)*N+1)
local x = mpfr.new(prec):const_pi()
print(x:tostring(10, N, mpfr.RNDD))
