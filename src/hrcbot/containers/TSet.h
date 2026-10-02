// TSet.h
// Fixed capacity dense set indexed by an integer id, plus its iterator.
// The original binary used TSet<Node, 127> (mangled TSetI4NodeLb0ELi127EE)
// to remember which navigation nodes were visited during a search.
#ifndef HRCBOT_CONTAINERS_TSET_H_
#define HRCBOT_CONTAINERS_TSET_H_

template <class T, int CAPACITY = 256>
class TSet
{
public:
	TSet() { Clear(); }

	void Clear()
	{
		for (int i = 0; i < CAPACITY; ++i)
			m_flags[i] = false;
		m_count = 0;
	}

	bool Contains(int id) const
	{
		if (id < 0 || id >= CAPACITY)
			return false;
		return m_flags[id];
	}

	bool Add(int id)
	{
		if (id < 0 || id >= CAPACITY)
			return false;
		if (!m_flags[id])
		{
			m_flags[id] = true;
			++m_count;
		}
		return true;
	}

	void Remove(int id)
	{
		if (id >= 0 && id < CAPACITY && m_flags[id])
		{
			m_flags[id] = false;
			--m_count;
		}
	}

	int Count() const { return m_count; }
	bool IsEmpty() const { return m_count == 0; }

	// Iterate over the contained ids.  The template parameter T is kept for
	// source compatibility with the original class signature.
	class TSetIterator
	{
	public:
		TSetIterator(TSet<T, CAPACITY> *set) : m_set(set), m_cursor(-1)
		{
			Advance();
		}
		bool HasNext() const { return m_cursor < CAPACITY; }
		int Next()
		{
			int current = m_cursor;
			Advance();
			return current;
		}
		void Reset()
		{
			m_cursor = -1;
			Advance();
		}

	private:
		void Advance()
		{
			++m_cursor;
			while (m_cursor < CAPACITY && !m_set->m_flags[m_cursor])
				++m_cursor;
		}
		TSet<T, CAPACITY> *m_set;
		int m_cursor;
	};

	TSetIterator GetIterator() { return TSetIterator(this); }

private:
	bool m_flags[CAPACITY];
	int m_count;
};

#endif // HRCBOT_CONTAINERS_TSET_H_
