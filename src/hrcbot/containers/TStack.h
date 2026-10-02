// TStack.h
// Simple LIFO stack, reconstructed from TStackI4RoomE (used by the room flood
// fill while building the navigation mesh).
#ifndef HRCBOT_CONTAINERS_TSTACK_H_
#define HRCBOT_CONTAINERS_TSTACK_H_

template <class T>
class TStack
{
public:
	TStack(int grow = 16)
		: m_items(NULL), m_count(0), m_capacity(0), m_grow(grow > 0 ? grow : 16)
	{
	}

	~TStack() { delete[] m_items; }

	int Count() const { return m_count; }
	bool IsEmpty() const { return m_count == 0; }

	bool Push(const T &value)
	{
		if (m_count == m_capacity)
		{
			int newcap = m_capacity ? m_capacity + m_grow : m_grow;
			T *repl = new T[newcap];
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

	T Pop()
	{
		if (m_count == 0)
			return T();
		return m_items[--m_count];
	}

	T &Top() { return m_items[m_count - 1]; }
	const T &Top() const { return m_items[m_count - 1]; }

	void Clear() { m_count = 0; }

private:
	T *m_items;
	int m_count;
	int m_capacity;
	int m_grow;

	TStack(const TStack &);
	TStack &operator=(const TStack &);
};

#endif // HRCBOT_CONTAINERS_TSTACK_H_
