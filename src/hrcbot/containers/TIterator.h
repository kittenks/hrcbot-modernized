// TIterator.h
// Common iterator interface used by every HRCBot generic container.
// Reconstructed from the Itanium RTTI symbols TIteratorI<...>EE present in
// the original 1.3.4 binaries.
#ifndef HRCBOT_CONTAINERS_TITERATOR_H_
#define HRCBOT_CONTAINERS_TITERATOR_H_

template <class T>
class TIterator
{
public:
	virtual ~TIterator() {}
	virtual bool HasNext() const = 0;
	virtual T *Next() = 0;
	virtual void Reset() = 0;
};

#endif // HRCBOT_CONTAINERS_TITERATOR_H_
