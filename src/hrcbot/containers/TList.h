// TList.h
// Ordered, array-backed generic list and its companion iterator.
// Reconstructed from TListI<...>EE / TListIteratorI<...>EE RTTI symbols.
#ifndef HRCBOT_CONTAINERS_TLIST_H_
#define HRCBOT_CONTAINERS_TLIST_H_

#include <stddef.h>
#include <string.h>
#include "TIterator.h"

template <class T>
class TList
{
public:
	TList(int grow = 16)
		: m_items(NULL), m_count(0), m_capacity(0), m_grow(grow > 0 ? grow : 16)
	{
	}

	~TList()
	{
		delete[] m_items;
	}

	int Count() const { return m_count; }
	bool IsEmpty() const { return m_count == 0; }

	void Clear()
	{
		m_count = 0;
	}

	T &Get(int index) { return m_items[index]; }
	const T &Get(int index) const { return m_items[index]; }

	T &operator[](int index) { return m_items[index]; }
	const T &operator[](int index) const { return m_items[index]; }

	int IndexOf(const T &value) const
	{
		for (int i = 0; i < m_count; ++i)
		{
			if (m_items[i] == value)
				return i;
		}
		return -1;
	}

	bool Contains(const T &value) const { return IndexOf(value) >= 0; }

	bool Ensure(int capacity)
	{
		if (capacity <= m_capacity)
			return true;
		int newcap = m_capacity ? m_capacity : m_grow;
		while (newcap < capacity)
			newcap += m_grow;
		T *replacement = new T[newcap];
		if (!replacement)
			return false;
		for (int i = 0; i < m_count; ++i)
			replacement[i] = m_items[i];
		delete[] m_items;
		m_items = replacement;
		m_capacity = newcap;
		return true;
	}

	bool Add(const T &value)
	{
		if (!Ensure(m_count + 1))
			return false;
		m_items[m_count++] = value;
		return true;
	}

	bool Insert(int index, const T &value)
	{
		if (index < 0)
			index = 0;
		if (index > m_count)
			index = m_count;
		if (!Ensure(m_count + 1))
			return false;
		for (int i = m_count; i > index; --i)
			m_items[i] = m_items[i - 1];
		m_items[index] = value;
		++m_count;
		return true;
	}

	bool RemoveAt(int index)
	{
		if (index < 0 || index >= m_count)
			return false;
		for (int i = index; i < m_count - 1; ++i)
			m_items[i] = m_items[i + 1];
		--m_count;
		return true;
	}

	bool Remove(const T &value)
	{
		int idx = IndexOf(value);
		if (idx < 0)
			return false;
		return RemoveAt(idx);
	}

	// Companion iterator over element addresses.
	class TListIterator : public TIterator<T>
	{
	public:
		TListIterator(TList<T> *list) : m_list(list), m_cursor(0) {}
		virtual bool HasNext() const { return m_cursor < m_list->Count(); }
		virtual T *Next()
		{
			if (m_cursor >= m_list->Count())
				return NULL;
			return &m_list->Get(m_cursor++);
		}
		virtual void Reset() { m_cursor = 0; }

	private:
		TList<T> *m_list;
		int m_cursor;
	};

	TListIterator GetIterator() { return TListIterator(this); }

private:
	T *m_items;
	int m_count;
	int m_capacity;
	int m_grow;

	// Non-copyable: the original containers owned their storage.
	TList(const TList &);
	TList &operator=(const TList &);
};

#endif // HRCBOT_CONTAINERS_TLIST_H_
