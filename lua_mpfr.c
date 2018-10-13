/*
 * Binding of MPFR to Lua
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <mpfr.h>

#include <lua.h>
#include <lauxlib.h>

#define VERSION "mpfr library for " LUA_VERSION \
		" (2018.10), MPFR " MPFR_VERSION_STRING
#define MPFR	"mpfr_t"


/* buffer size for mpfr_get_str. see its doc for the formula. */
static size_t _outbufsize(mpfr_t z, int b, size_t n)
{
	if (!n)
		n = ceil(mpfr_get_prec(z) * log(2.0) / log(b)) + 1;
	return (n < 5) ? 7 : n + 2;
}

static int _opt_base(lua_State *L, int i)
{
	lua_Integer b;

	b = luaL_optinteger(L, i, 10);
	luaL_argcheck(L, 2 <= b && b <= 62,
		i, "base must be between 2 and 62");
	return b;
}

static int _opt_rnd(lua_State *L, int i)
{
	lua_Integer r;

	r = luaL_optinteger(L, i, mpfr_get_default_rounding_mode());
	return r;
}

/* tostring(self, [base], [n], [rnd]) */
static int fr_tostring(lua_State *L)
{
	luaL_Buffer B;
	char *s, *p;
	mpfr_exp_t e;
	int b;
	size_t n;
	mpfr_ptr z;
	mpfr_rnd_t r;
	size_t sz, len;

	z = luaL_checkudata(L, 1, MPFR);

	b = _opt_base(L, 2);
	n = luaL_optinteger(L, 3, 0);
	r = _opt_rnd(L, 4);

	/* special cases */
	if (!mpfr_number_p(z)) {
		char buf[7];
		mpfr_get_str(buf, &e, b, n, z, r);
		lua_pushstring(L, buf);
		return 1;
	} else if (mpfr_zero_p(z)) {
		lua_pushstring(L, mpfr_signbit(z) ? "-0" : "0");
		return 1;
	}

	sz = _outbufsize(z, b, n) + 1; /* +1 for the decimal point */
	s = luaL_buffinitsize(L, &B, sz);
	p = s + 1;
	mpfr_get_str(p, &e, b, n, z, r);

	len = strlen(p); /* actual length from mpfr_get_str */
	if (*p == '-')
		*s++ = *p++; /* skip minus sign */

	/* insert decimal point */
	s[0] = *p;
	s[1] = '.';
	luaL_addsize(&B, len + 1);

	if (--e) { /* append exponent */
		luaL_addchar(&B, (b > 10) ? '@' : 'e');
		lua_pushinteger(L, e);
		luaL_addvalue(&B);
	}

	luaL_pushresult(&B);
	return 1;
}


/* tonumber(self, [rnd]) */
static int fr_tonumber(lua_State *L)
{
	mpfr_ptr z;
	mpfr_rnd_t r;

	z = luaL_checkudata(L, 1, MPFR);
	r = _opt_rnd(L, 2);
	if (sizeof (long) <= sizeof (lua_Integer) &&
			mpfr_integer_p(z) && mpfr_fits_slong_p(z, r))
		lua_pushinteger(L, mpfr_get_si(z, r));
	else
		lua_pushnumber(L, mpfr_get_d(z, r));
	return 1;
}


static int fr_gc(lua_State *L)
{
	mpfr_ptr z;

	z = luaL_checkudata(L, 1, MPFR);
	mpfr_clear(z);
	return 0;
}



static lua_Integer _check_prec(lua_State *L, int i)
{
	lua_Integer prec;

	prec = luaL_checkinteger(L, i);
	if (!(MPFR_PREC_MIN <= prec && prec <= MPFR_PREC_MAX))
		luaL_argerror(L, i, lua_pushfstring(L,
			"precision must be between %I and %I",
			(lua_Integer) MPFR_PREC_MIN,
			(lua_Integer) MPFR_PREC_MAX));
	return prec;
}

/* new([prec]) : mpfr_t */
static int fr_new(lua_State *L)
{
	mpfr_ptr z;
	lua_Integer prec = 0;

	if (!lua_isnoneornil(L, 1))
		prec = _check_prec(L, 1);

	z = lua_newuserdata(L, sizeof (*z));
	if (prec)
		mpfr_init2(z, _check_prec(L, 1));
	else
		mpfr_init(z);
	luaL_setmetatable(L, MPFR);
	return 1;
}

#define V_LONG 0
#define V_DOUBLE 1
#define V_MPFR 2

union value {
	long i;
	double d;
	mpfr_ptr fr;
};

static int _check_value(lua_State *L, int i, union value *v)
{
	lua_Integer x;
	int isint;

	if (lua_isnumber(L, i)) {
		x = lua_tointegerx(L, i, &isint);
		if (isint && (v->i = x) == x)
			return V_LONG;
		v->d = lua_tonumber(L, i);
		return V_DOUBLE;
	}
	v->fr = luaL_checkudata(L, i, MPFR);
	return V_MPFR;
}

/* set(self, number, [rnd])
 * set(self, string, [base], [rnd])
 */
static int fr_set(lua_State *L)
{
	mpfr_ptr z;
	union value v;
	mpfr_rnd_t r;

	z = luaL_checkudata(L, 1, MPFR);
	if (lua_isstring(L, 2)) {
		r = _opt_rnd(L, 4);
		if (mpfr_set_str(z, lua_tostring(L, 2),
				_opt_base(L, 3), r) != 0)
			luaL_argerror(L, 2,
				"not a valid number in given base");
	} else {
		r = _opt_rnd(L, 3);
		switch (_check_value(L, 2, &v)) {
		case V_LONG:
			mpfr_set_si(z, v.i, r);
			break;
		case V_DOUBLE:
			mpfr_set_d(z, v.d, r);
			break;
		case V_MPFR:
			mpfr_set(z, v.fr, r);
			break;
		}
	}
	lua_settop(L, 1);
	return 1;
}

static int fr_set_nan(lua_State *L)
{
	mpfr_set_nan(luaL_checkudata(L, 1, MPFR));
	lua_settop(L, 1);
	return 1;
}

static int fr_set_inf(lua_State *L)
{
	mpfr_set_inf(luaL_checkudata(L, 1, MPFR), luaL_optinteger(L, 2, 0));
	lua_settop(L, 1);
	return 1;
}

static int fr_set_zero(lua_State *L)
{
	mpfr_set_zero(luaL_checkudata(L, 1, MPFR), luaL_optinteger(L, 2, 0));
	lua_settop(L, 1);
	return 1;
}


/* -> fr */
static int fr_fn0(lua_State *L)
{
	int (*fn)(mpfr_ptr, mpfr_rnd_t);

	fn = lua_touserdata(L, lua_upvalueindex(1));
	(*fn)(luaL_checkudata(L, 1, MPFR), _opt_rnd(L, 2));
	lua_settop(L, 1);
	return 1;
}

struct fn0_reg {
	const char *name;
	int (*fn)(mpfr_ptr, mpfr_rnd_t);
};

#define FN_(lname, cname) {lname, cname}
#define FN(name) FN_(#name, mpfr_##name)

static const struct fn0_reg _fn0_reg[] = {
	FN(const_log2),
	FN(const_pi),
	FN(const_euler),
	FN(const_catalan),
	{0, 0}
};

static void _reg_fn0(lua_State *L, const struct fn0_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn0, 1);
		lua_setfield(L, -2, r->name);
	}
}


/* fr -> fr */
static int fr_fn1(lua_State *L)
{
	int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

	fn = lua_touserdata(L, lua_upvalueindex(1));
	(*fn)(luaL_checkudata(L, 1, MPFR), luaL_checkudata(L, 2, MPFR),
		_opt_rnd(L, 3));
	lua_settop(L, 1);
	return 1;
}

struct fn1_reg {
	const char *name;
	int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
};

static const struct fn1_reg _fn1_reg[] = {
	FN(sqr),
	FN(rec_sqrt),
	FN(cbrt),
	FN(abs),
	FN(neg),
	FN(log),
	FN(log2),
	FN(log10),
	FN(log1p),
	FN(exp),
	FN(exp2),
	FN(exp10),
	FN(expm1),
	FN(cos),
	FN(sin),
	FN(tan),
	FN(sec),
	FN(csc),
	FN(cot),
	FN(acos),
	FN(asin),
	FN(atan),
	FN(cosh),
	FN(sinh),
	FN(tanh),
	FN(sech),
	FN(csch),
	FN(coth),
	FN(acosh),
	FN(asinh),
	FN(atanh),
	FN(eint),
	FN(li2),
	FN(gamma),
	FN(lngamma),
	FN(digamma),
	FN(erf),
	FN(erfc),
	FN(j0),
	FN(j1),
	FN(y0),
	FN(y1),
	FN(ai),
	FN(rint),
	FN(rint_ceil),
	FN(rint_floor),
	FN(rint_round),
	FN(rint_trunc),
	FN(frac),
	{0, 0}
};

static void _reg_fn1(lua_State *L, const struct fn1_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn1, 1);
		lua_setfield(L, -2, r->name);
	}
}


/* fr|ui -> fr */
static int fr_fn1u(lua_State *L)
{
	mpfr_ptr z;
	mpfr_rnd_t r;

	z = luaL_checkudata(L, 1, MPFR);
	r = _opt_rnd(L, 3);
	if (lua_isinteger(L, 2)) {
		lua_Integer i;
		int (*fn)(mpfr_ptr, unsigned long, mpfr_rnd_t);

		i = lua_tointeger(L, 2);
		luaL_argcheck(L, 0 <= i && i <= ULONG_MAX, 2,
			"out of range of unsigned long");
		fn = lua_touserdata(L, lua_upvalueindex(2));
		(*fn)(z, i, r);
	} else {
		int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

		fn = lua_touserdata(L, lua_upvalueindex(1));
		(*fn)(z, luaL_checkudata(L, 2, MPFR), r);
	}
	lua_settop(L, 1);
	return 1;
}

struct fn1u_reg {
	const char *name;
	int (*fr)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
	int (*ui)(mpfr_ptr, unsigned long, mpfr_rnd_t);
};

#define FN2(name) {#name, mpfr_##name, mpfr_##name##_ui}
static const struct fn1u_reg _fn1u_reg[] = {
	FN2(sqrt),
	FN2(zeta),
	{0, 0}
};

static void _reg_fn1u(lua_State *L, const struct fn1u_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fr);
		lua_pushlightuserdata(L, r->ui);
		lua_pushcclosure(L, fr_fn1u, 2);
		lua_setfield(L, -2, r->name);
	}
}


/* fr -> fr, fr */
static int fr_fn12(lua_State *L)
{
	int (*fn)(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
	mpfr_ptr x, y, z;
	mpfr_rnd_t r;

	x = luaL_checkudata(L, 1, MPFR);
	y = luaL_checkudata(L, 2, MPFR);
	z = luaL_checkudata(L, 3, MPFR);
	r = _opt_rnd(L, 4);
	fn = lua_touserdata(L, lua_upvalueindex(1));
	(*fn)(x, y, z, r);
	lua_settop(L, 2);
	return 2;
}

struct fn12_reg {
	const char *name;
	int (*fn)(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
};

static const struct fn12_reg _fn12_reg[] = {
	FN(modf),
	FN(sin_cos),
	FN(sinh_cosh),
	{0, 0}
};

static void _reg_fn12(lua_State *L, const struct fn12_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn12, 1);
		lua_setfield(L, -2, r->name);
	}
}


static int fr_fac(lua_State *L)
{
	mpfr_ptr z;
	lua_Integer i;
	mpfr_rnd_t r;

	z = luaL_checkudata(L, 1, MPFR);
	i = lua_tointeger(L, 2);
	luaL_argcheck(L, 0 <= i && i <= ULONG_MAX, 2,
		"out of range of unsigned long");
	r = _opt_rnd(L, 3);
	mpfr_fac_ui(z, i, r);
	lua_settop(L, 1);
	return 1;
}


static int fr_sgn(lua_State *L)
{
	mpfr_ptr z;

	z = luaL_checkudata(L, 1, MPFR);
	lua_pushinteger(L, mpfr_sgn(z));
	return 1;
}


/* fr -> bool */
static int fr_fn1p(lua_State *L)
{
	int (*fn)(mpfr_srcptr);

	fn = lua_touserdata(L, lua_upvalueindex(1));
	lua_pushboolean(L, (*fn)(luaL_checkudata(L, 1, MPFR)));
	return 1;
}

struct fn1p_reg {
	const char *name;
	int (*fn)(mpfr_srcptr);
};

static const struct fn1p_reg _fn1p_reg[] = {
	FN(nan_p),
	FN(inf_p),
	FN(number_p),
	FN(zero_p),
	FN(regular_p),
	FN(integer_p),
	{0, 0}
};

static void _reg_fn1p(lua_State *L, const struct fn1p_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn1p, 1);
		lua_setfield(L, -2, r->name);
	}
}



/* fr,fr|fr,si|fr,d|si,fr|d,fr -> fr */
static int fr_fn2(lua_State *L)
{
	union {
		int (*fr_fr)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
		int (*fr_si)(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
		int (*fr_d)(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
		int (*si_fr)(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
		int (*d_fr)(mpfr_ptr, double, mpfr_srcptr, mpfr_rnd_t);
	} fn;
	mpfr_ptr z;
	mpfr_rnd_t r;
	int xtype;
	union value x, y;

	z = luaL_checkudata(L, 1, MPFR);
	xtype = _check_value(L, 2, &x);
	r = _opt_rnd(L, 4);

	if (xtype == V_MPFR) {
		switch(_check_value(L, 3, &y)) {
		case V_LONG:
			fn.fr_si = lua_touserdata(L, lua_upvalueindex(2));
			(*fn.fr_si)(z, x.fr, y.i, r);
			break;
		case V_DOUBLE:
			fn.fr_d = lua_touserdata(L, lua_upvalueindex(3));
			(*fn.fr_d)(z, x.fr, y.d, r);
			break;
		case V_MPFR:
			fn.fr_fr = lua_touserdata(L, lua_upvalueindex(1));
			(*fn.fr_fr)(z, x.fr, y.fr, r);
			break;
		}
	} else {
		y.fr = luaL_checkudata(L, 3, MPFR);
		switch (xtype) {
		case V_LONG:
			fn.si_fr = lua_touserdata(L, lua_upvalueindex(4));
			if (fn.si_fr) {
				(*fn.si_fr)(z, x.i, y.fr, r);
			} else {
				fn.fr_si = lua_touserdata(L,
					lua_upvalueindex(2));
				(*fn.fr_si)(z, y.fr, x.i, r);
			}
			break;
		case V_DOUBLE:
			fn.d_fr = lua_touserdata(L, lua_upvalueindex(5));
			if (fn.d_fr) {
				(*fn.d_fr)(z, x.d, y.fr, r);
			} else {
				fn.fr_d = lua_touserdata(L,
					lua_upvalueindex(3));
				(*fn.fr_d)(z, y.fr, x.d, r);
			}
			break;
		}
	}
	lua_settop(L, 1);

	return 1;
}


struct fn2_reg {
	const char *name;
	int (*fr_fr)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
	int (*fr_si)(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
	int (*fr_d)(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
	int (*si_fr)(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
	int (*d_fr)(mpfr_ptr, double, mpfr_srcptr, mpfr_rnd_t);
};

#define FN3(name) {#name, mpfr_##name, mpfr_##name##_si, mpfr_##name##_d}
#define FN5(name) {#name, mpfr_##name, mpfr_##name##_si, mpfr_##name##_d, \
	mpfr_si_##name, mpfr_d_##name}

static const struct fn2_reg _fn2_reg[] = {
	FN3(add),
	FN5(sub),
	FN3(mul),
	FN5(div),
	{0, 0}
};

static void _reg_fn2(lua_State *L, const struct fn2_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fr_fr);
		lua_pushlightuserdata(L, r->fr_si);
		lua_pushlightuserdata(L, r->fr_d);
		if (r->si_fr) {
			lua_pushlightuserdata(L, r->si_fr);
			lua_pushlightuserdata(L, r->d_fr);
			lua_pushcclosure(L, fr_fn2, 5);
		} else {
			lua_pushcclosure(L, fr_fn2, 3);
		}
		lua_setfield(L, -2, r->name);
	}
}


static int fr_pow(lua_State *L)
{
	mpfr_ptr x, y, z;
	mpfr_rnd_t r;
	lua_Integer i1, i2;
	int isint1, isint2;

	z = luaL_checkudata(L, 1, MPFR);
	i1 = lua_tointegerx(L, 2, &isint1);
	i2 = lua_tointegerx(L, 3, &isint2);
	r = _opt_rnd(L, 4);
	if (isint1) {
		luaL_argcheck(L, 0 <= i1 && i1 <= ULONG_MAX, 2,
			"out of range of unsigned long");
		if (isint2) {
			luaL_argcheck(L, 0 <= i2 && i2 <= ULONG_MAX, 2,
				"out of range of unsigned long");
			mpfr_ui_pow_ui(z, i1, i2, r);
		} else {
			y = luaL_checkudata(L, 3, MPFR);
			mpfr_ui_pow(z, i1, y, r);
		}
	} else {
		x = luaL_checkudata(L, 2, MPFR);
		if (isint2) {
			if (i2 < 0) {
				luaL_argcheck(L, i2 >= LONG_MIN, 2,
					"out of range of long");
				mpfr_pow_si(z, x, i2, r);
			} else {
				luaL_argcheck(L, i2 <= ULONG_MAX, 2,
					"out of range of unsigned long");
				mpfr_pow_ui(z, x, i2, r);
			}
		} else {
			y = luaL_checkudata(L, 3, MPFR);
			mpfr_pow(z, x, y, r);
		}
	}
	lua_settop(L, 1);
	return 1;
}

static int fr_root(lua_State *L)
{
	mpfr_ptr x, z;
	mpfr_rnd_t r;
	lua_Integer k;

	z = luaL_checkudata(L, 1, MPFR);
	x = luaL_checkudata(L, 2, MPFR);
	k = luaL_checkinteger(L, 3);
	luaL_argcheck(L, 0 <= k && k <= ULONG_MAX, 3,
		"out of range of unsigned long");
	r = _opt_rnd(L, 4);

	mpfr_root(z, x, k, r);

	lua_settop(L, 1);
	return 1;
}


/* fr,fr -> fr */
static int fr_fn2f(lua_State *L)
{
	int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

	fn = lua_touserdata(L, lua_upvalueindex(1));
	(*fn)(luaL_checkudata(L, 1, MPFR), luaL_checkudata(L, 2, MPFR),
		luaL_checkudata(L, 3, MPFR), _opt_rnd(L, 4));
	lua_settop(L, 1);
	return 1;
}

struct fn2f_reg {
	const char *name;
	int (*fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
};

static const struct fn2f_reg _fn2f_reg[] = {
	FN(fmod),
	FN(remainder),
	FN(atan2),
	FN(agm),
	FN(hypot),
	FN(min),
	FN(max),
	{0, 0}
};

static void _reg_fn2f(lua_State *L, const struct fn2f_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn2f, 1);
		lua_setfield(L, -2, r->name);
	}
}


/* si,fr -> fr */
static int fr_fn2n(lua_State *L)
{
	int (*fn)(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
	mpfr_ptr x, z;
	lua_Integer n;
	mpfr_rnd_t r;

	z = luaL_checkudata(L, 1, MPFR);
	n = luaL_checkinteger(L, 2);
	luaL_argcheck(L, LONG_MIN <= n && n <= LONG_MAX, 2,
		"out of range of long");
	x = luaL_checkudata(L, 3, MPFR);
	r = _opt_rnd(L, 4);
	fn = lua_touserdata(L, lua_upvalueindex(1));

	(*fn)(z, n, x, r);
	lua_settop(L, 1);
	return 1;
}

struct fn2n_reg {
	const char *name;
	int (*fn)(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
};

static const struct fn2n_reg _fn2n_reg[] = {
	FN(jn),
	FN(yn),
	{0, 0}
};

static void _reg_fn2n(lua_State *L, const struct fn2n_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn2n, 1);
		lua_setfield(L, -2, r->name);
	}
}


static int fr_cmp(lua_State *L)
{
	mpfr_ptr x;
	union value y;
	int result = 0;

	x = luaL_checkudata(L, 1, MPFR);
	switch (_check_value(L, 2, &y)) {
	case V_LONG:
		result = mpfr_cmp_si(x, y.i);
		break;
	case V_DOUBLE:
		result = mpfr_cmp_d(x, y.d);
		break;
	case V_MPFR:
		result = mpfr_cmp(x, y.fr);
		break;
	}
	lua_pushinteger(L, result);
	return 1;
}

static int fr_cmpabs(lua_State *L)
{
	mpfr_ptr x, y;

	x = luaL_checkudata(L, 1, MPFR);
	y = luaL_checkudata(L, 2, MPFR);
	lua_pushinteger(L, mpfr_cmpabs(x, y));
	return 1;
}


/* fr,fr -> bool */
static int fr_fn2p(lua_State *L)
{
	int (*fn)(mpfr_srcptr, mpfr_srcptr);

	fn = lua_touserdata(L, lua_upvalueindex(1));
	lua_pushboolean(L, (*fn)(luaL_checkudata(L, 1, MPFR),
		luaL_checkudata(L, 2, MPFR)));
	return 1;
}

struct fn2p_reg {
	const char *name;
	int (*fn)(mpfr_srcptr, mpfr_srcptr);
};

static const struct fn2p_reg _fn2p_reg[] = {
	FN(greater_p),
	FN(greaterequal_p),
	FN(less_p),
	FN(lessequal_p),
	FN(equal_p),
	FN(lessgreater_p),
	FN(unordered_p),
	{0, 0}
};

static void _reg_fn2p(lua_State *L, const struct fn2p_reg *r)
{
	for (; r->name; r++) {
		lua_pushlightuserdata(L, r->fn);
		lua_pushcclosure(L, fr_fn2p, 1);
		lua_setfield(L, -2, r->name);
	}
}


static int fr_fma(lua_State *L)
{
	mpfr_fma(luaL_checkudata(L, 1, MPFR),
		luaL_checkudata(L, 2, MPFR),
		luaL_checkudata(L, 3, MPFR),
		luaL_checkudata(L, 4, MPFR),
		_opt_rnd(L, 5));
	lua_settop(L, 1);
	return 1;
}

static int fr_fms(lua_State *L)
{
	mpfr_fms(luaL_checkudata(L, 1, MPFR),
		luaL_checkudata(L, 2, MPFR),
		luaL_checkudata(L, 3, MPFR),
		luaL_checkudata(L, 4, MPFR),
		_opt_rnd(L, 5));
	lua_settop(L, 1);
	return 1;
}


static int fr_prec_round(lua_State *L)
{
	mpfr_prec_round(luaL_checkudata(L, 1, MPFR),
		_check_prec(L, 2), _opt_rnd(L, 3));
	lua_settop(L, 1);
	return 1;
}

static int fr_can_round(lua_State *L)
{
	lua_pushboolean(L, mpfr_can_round(
		luaL_checkudata(L, 1, MPFR),	/* z */
		luaL_checkinteger(L, 2),	/* err */
		luaL_checkinteger(L, 3),	/* r1 */
		luaL_checkinteger(L, 4),	/* r2 */
		_check_prec(L, 5)));		/* prec */
	return 1;
}


static int fr_set_prec(lua_State *L)
{
	mpfr_set_prec(luaL_checkudata(L, 1, MPFR), _check_prec(L, 2));
	return 0;
}

static int fr_get_prec(lua_State *L)
{
	mpfr_ptr z;

	z = luaL_checkudata(L, 1, MPFR);
	lua_pushinteger(L, mpfr_get_prec(z));
	return 1;
}

static int fr_min_prec(lua_State *L)
{
	lua_pushinteger(L, mpfr_min_prec(luaL_checkudata(L, 1, MPFR)));
	return 1;
}

static int fr_copysign(lua_State *L)
{
	mpfr_ptr x, y, z;
	mpfr_rnd_t r;

	luaL_checkudata(L, 1, MPFR),
	luaL_checkudata(L, 2, MPFR),
	luaL_checkudata(L, 3, MPFR),
	_opt_rnd(L, 4),

	mpfr_copysign(z, x, y, r);
	lua_settop(L, 1);
	return 1;
}


static int fr_free_cache(lua_State *L)
{
	mpfr_free_cache();
	return 0;
}

static int fr_set_default_prec(lua_State *L)
{
	mpfr_set_default_prec(_check_prec(L, 1));
	return 0;
}

static int fr_get_default_prec(lua_State *L)
{
	lua_pushinteger(L, mpfr_get_default_prec());
	return 1;
}

static int fr_set_default_rounding_mode(lua_State *L)
{
	mpfr_set_default_rounding_mode(luaL_checkinteger(L, 1));
	return 0;
}

static int fr_get_default_rounding_mode(lua_State *L)
{
	lua_pushinteger(L, mpfr_get_default_rounding_mode());
	return 1;
}

static const luaL_Reg _reg[] = 
{
	{"__tostring", fr_tostring},
	{"__gc", fr_gc},
	{"new", fr_new},
	{"tostring", fr_tostring},
	{"tonumber", fr_tonumber},
	{"set", fr_set},
	{"set_nan", fr_set_nan},
	{"set_inf", fr_set_inf},
	{"set_zero", fr_set_zero},
	{"pow", fr_pow},
	{"root", fr_root},
	{"cmp", fr_cmp},
	{"cmpabs", fr_cmpabs},
	{"sgn", fr_sgn},
	{"fac", fr_fac},
	{"fma", fr_fma},
	{"fms", fr_fms},
	{"prec_round", fr_prec_round},
	{"can_round", fr_can_round},
	{"set_prec", fr_set_prec},
	{"get_prec", fr_get_prec},
	{"min_prec", fr_min_prec},
	{"copysign", fr_copysign},
	{"free_cache", fr_free_cache},
	{"set_default_prec", fr_set_default_prec},
	{"get_default_prec", fr_get_default_prec},
	{"set_default_rounding_mode", fr_set_default_rounding_mode},
	{"get_default_rounding_mode", fr_get_default_rounding_mode},
	{0, 0},
};

static void _reg_rnd(lua_State *L)
{
	static const mpfr_rnd_t rnd[] = {
		MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
	};
	int n = 5;
	while (n--) {
		lua_pushinteger(L, rnd[n]);
		lua_setfield(L, -2, mpfr_print_rnd_mode(rnd[n]) + 5); /* skip MPFR_ */
	}
}

LUALIB_API int luaopen_mpfr(lua_State *L)
{
	luaL_newmetatable(L, MPFR);
	luaL_setfuncs(L, _reg, 0);
	lua_pushliteral(L, VERSION);
	lua_setfield(L, -2, "version");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	_reg_fn0(L, _fn0_reg);
	_reg_fn1(L, _fn1_reg);
	_reg_fn12(L, _fn12_reg);
	_reg_fn1u(L, _fn1u_reg);
	_reg_fn1p(L, _fn1p_reg);
	_reg_fn2(L, _fn2_reg);
	_reg_fn2f(L, _fn2f_reg);
	_reg_fn2n(L, _fn2n_reg);
	_reg_fn2p(L, _fn2p_reg);
	_reg_rnd(L);

	return 1;
}
