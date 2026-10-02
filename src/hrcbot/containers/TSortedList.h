// TSortedList.h
// Ordered list that keeps its elements sorted using a user supplied compare
// function.  Reconstructed from TSortedListI4NodeE.
#ifndef HRCBOT_CONTAINERS_TSORTEDLIST_H_
#define HRCBOT_CONTAINERS_TSORTEDLIST_H_

#include "TList.h"

template <class T>
class TSortedList : public TList<T>
{
public:
	typedef int (*CompareFn)(const T &a, const T &b);

	TSortedList(CompareFn cmp, int grow = 16)
		: TList<T>(grow), m_compare(cmp)
	{
	}

	// Insert keeping the list sorted.  Equal elements are grouped.
	int AddSorted(const T &value)
	{
		int lo = 0, hi = this->Count();
		while (lo < hi)
		{
			int mid = (lo + hi) / 2;
			if (m_compare(this->Get(mid), value) < 0)
				lo = mid + 1;
			else
				hi = mid;
		}
		this->Insert(lo, value);
		return lo;
	}

private:
	CompareFn m_compare;
};

#endif // HRCBOT_CONTAINERS_TSORTEDLIST_H_
