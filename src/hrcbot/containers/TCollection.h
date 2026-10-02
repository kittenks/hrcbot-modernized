// TCollection.h
// Owning collection of heap allocated objects.  The RTTI in the original
// binary showed TCollection specializations for Arc, IBot, Item, Node, Room,
// Object and Player.  Those objects are polymorphic, so the collection owns
// pointers and deletes them on Clear()/destruction.
#ifndef HRCBOT_CONTAINERS_TCOLLECTION_H_
#define HRCBOT_CONTAINERS_TCOLLECTION_H_

#include "TIterator.h"

template <class T>
class TCollection
{
public:
	TCollection(int grow = 16)
		: m_items(NULL), m_count(0), m_capacity(0), m_grow(grow > 0 ? grow : 16)
	{
	}

	~TCollection() { DeleteAll(); }

	int Count() const { return m_count; }
	bool IsEmpty() const { return m_count == 0; }

	T *Get(int index) const
	{
		if (index < 0 || index >= m_count)
			return NULL;
		return m_items[index];
	}

	T *operator[](int index) const { return Get(index); }

	bool Add(T *value)
	{
		if (!value)
			return false;
		if (m_count == m_capacity)
		{
			int newcap = m_capacity ? m_capacity + m_grow : m_grow;
			T **repl = new T *[newcap];
			if (!repl)
				return false;
			for (int i = 0; i < m_count; ++i)
				repl[i] = m_items[i];
			delete[] m_items;
			m_items = repl;
			m_capacity = newcap;
		}
		m_items[m_count++] = value;
		return true;
	}

	bool Remove(T *value, bool deleteObject = true)
	{
		for (int i = 0; i < m_count; ++i)
		{
			if (m_items[i] == value)
			{
				if (deleteObject)
					delete value;
				for (int j = i; j < m_count - 1; ++j)
					m_items[j] = m_items[j + 1];
				--m_count;
				return true;
			}
		}
		return false;
	}

	void DeleteAll()
	{
		for (int i = 0; i < m_count; ++i)
			delete m_items[i];
		delete[] m_items;
		m_items = NULL;
		m_count = m_capacity = 0;
	}

	void DetachAll()
	{
		// Drop ownership without deleting (used during map teardown where the
		// objects are owned by another subsystem).
		m_count = 0;
	}

	class TCollectionIterator : public TIterator<T>
	{
	public:
		TCollectionIterator(TCollection<T> *coll) : m_coll(coll), m_cursor(0) {}
		virtual bool HasNext() const { return m_cursor < m_coll->Count(); }
		virtual T *Next()
		{
			if (m_cursor >= m_coll->Count())
				return NULL;
			return m_coll->Get(m_cursor++);
		}
		virtual void Reset() { m_cursor = 0; }

	private:
		TCollection<T> *m_coll;
		int m_cursor;
	};

	TCollectionIterator GetIterator() { return TCollectionIterator(this); }

private:
	T **m_items;
	int m_count;
	int m_capacity;
	int m_grow;

	TCollection(const TCollection &);
	TCollection &operator=(const TCollection &);
};

#endif // HRCBOT_CONTAINERS_TCOLLECTION_H_
