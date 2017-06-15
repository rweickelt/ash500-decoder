#ifndef ARRAYLIST_
#define ARRAYLIST_

#include <stdint.h>
#include <assert.h>

template<typename T, int32_t maxLength>
class ArrayList
{
public:
    typedef int32_t Index;
    typedef int32_t LengthT;

    enum {
        InvalidIndex = -1
    };

    void append(const T& item);
    const T& at(Index index) const;
    Index indexOf(const T& item) const;
    LengthT length() const;

    T& operator[](Index index);
    const T& operator[](Index index) const;

private:
    T m_data[maxLength];
    Index m_length;
};


template<typename T, int32_t maxLength>
void ArrayList<T, maxLength>::append(const T& item)
{
    assert(m_length < maxLength);
    m_data[m_length] = item;
    m_length++;
}

template<typename T, int32_t maxLength>
const T& ArrayList<T, maxLength>::at(ArrayList<T,maxLength>::Index index) const
{
    assert(index < m_length);
    return m_data[index];
}

template<typename T, int32_t maxLength>
ArrayList<T,maxLength>::Index ArrayList<T, maxLength>::indexOf(const T& item) const
{
    for (Index i = 0; i < m_length; i++)
    {
        if (m_data[i] == item)
        {
            return i;
        }
    }
    return InvalidIndex;
}

template<typename T, int32_t maxLength>
ArrayList<T,maxLength>::LengthT ArrayList<T, maxLength>::length() const
{
    return m_length;
}

template<typename T, int32_t maxLength>
T& ArrayList<T, maxLength>::operator[](Index index)
{
    assert(index < m_length);
    return m_data[index];
}

template<typename T, int32_t maxLength>
const T& ArrayList<T, maxLength>::operator[](Index index) const
{
    assert(index < m_length);
    return m_data[index];
}



#endif /* ARRAYLIST_ */
