#ifndef STRAITX_SPAN_HPP
#define STRAITX_SPAN_HPP 

#include "platform/types.hpp"
#include "core/assert.hpp"

// it has no idea about allocations, object lifetime and stuff
template <typename Type, typename SizeType = size_t>
class Span{
public:
    typedef Type* Iterator;
private:
    Type *m_Pointer = nullptr;
    SizeType m_Size = 0;
public:

    Span() = default;

    Span(const Span &other) = default;

    Span(Type *pointer, SizeType size):
        m_Pointer(pointer),
        m_Size(size)
    {}

    template<size_t ArraySize>
    Span(Type (&array)[ArraySize]):
        m_Pointer(&array[0]),
        m_Size(ArraySize)
    {}

    Type &operator[](SizeType index)const{
        SX_CORE_ASSERT(index < m_Size, "Span: can't index more that Span::Size elements");
        return m_Pointer[index]; 
    }

    Span &operator=(const Span &other) = default;

    SizeType Size()const{
        return m_Size;
    }
    // XXX Change it to Data()
    Type *Pointer()const{
        return m_Pointer;
    }

    Iterator begin()const{
        return Pointer();
    }

    Iterator end()const{
        return Pointer() + Size();
    }
};

#endif //STRAITX_SPAN_HPP