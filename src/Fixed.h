#ifndef FIXED_H
#define FIXED_H

class UFixed {
private:
	int num;
	const static int fracbits = 16;

public:
	UFixed() : num(0) {}
	UFixed(const UFixed& n) : num(n.num) {}
	UFixed(float n) : num( static_cast<int>(n * (float)(1 << fracbits))) {}
	UFixed(double n) : num( static_cast<int>(n * (double)(1 << fracbits))) {}
	UFixed(int n) : num( (int)(n << fracbits)) {}

	operator float() { return num / (float)(1 << fracbits); }
	operator int() { return num >> fracbits; }

	UFixed& operator =(const UFixed& n) { num = n.num; return *this; }
	UFixed& operator +=(UFixed n) { num += n.num; return *this; }
	UFixed& operator -=(UFixed n) { num -= n.num; return *this; }

	UFixed operator +(const UFixed& n) const { UFixed sum; sum.num = num + n.num; return sum; }
	UFixed operator -(const UFixed& n) const { UFixed diff; diff.num = num - n.num; return diff; }
	UFixed operator /(int n) const { UFixed div; div.num = num / (int)n; return div; }
	UFixed operator *(int n) const { UFixed mul; mul.num = num * (int)n; return mul; }

	bool operator ==(const UFixed& n) const { return num == n.num; }
	bool operator !=(const UFixed& n) const { return num != n.num; }
	bool operator <(const UFixed& n) const { return num < n.num; }
	bool operator >(const UFixed& n) const { return num > n.num; }
	bool operator <=(const UFixed& n) const { return num <= n.num; }
	bool operator >=(const UFixed& n) const { return num >= n.num; }
};
typedef class UFixed UFixed;

#endif
