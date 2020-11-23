#pragma once
#include <stdint.h>

int32_t android_atomic_inc(volatile int32_t* addr);

int32_t android_atomic_dec(volatile int32_t* addr);

// ---------------------------------------------------------------------------
namespace android {

    template<typename T> class wp;

    // ---------------------------------------------------------------------------

#define COMPARE(_op_)                                           \
inline bool operator _op_ (const sp<T>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
inline bool operator _op_ (const wp<T>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
inline bool operator _op_ (const T* o) const {                  \
    return m_ptr _op_ o;                                        \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const sp<U>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const wp<U>& o) const {              \
    return m_ptr _op_ o.m_ptr;                                  \
}                                                               \
template<typename U>                                            \
inline bool operator _op_ (const U* o) const {                  \
    return m_ptr _op_ o;                                        \
}

// ---------------------------------------------------------------------------

    class RefBase
    {
    public:
        void            incStrong(const void* id) const;
        void            decStrong(const void* id) const;

        void            forceIncStrong(const void* id) const;

        //! DEBUGGING ONLY: Get current strong ref count.
        int32_t         getStrongCount() const;

        class weakref_type
        {
        public:
            RefBase* refBase() const;

            void                incWeak(const void* id);
            void                decWeak(const void* id);

            bool                attemptIncStrong(const void* id);

            //! This is only safe if you have set OBJECT_LIFETIME_FOREVER.
            bool                attemptIncWeak(const void* id);

            //! DEBUGGING ONLY: Get current weak ref count.
            int32_t             getWeakCount() const;

        };

        weakref_type* createWeak(const void* id) const;

        weakref_type* getWeakRefs() const;



    protected:
        RefBase();
        virtual                 ~RefBase();

        //! Flags for extendObjectLifetime()
        enum {
            OBJECT_LIFETIME_WEAK = 0x0001,
            OBJECT_LIFETIME_FOREVER = 0x0003
        };

        void            extendObjectLifetime(int32_t mode);

        //! Flags for onIncStrongAttempted()
        enum {
            FIRST_INC_STRONG = 0x0001
        };

        virtual void            onFirstRef();
        virtual void            onLastStrongRef(const void* id);
        virtual bool            onIncStrongAttempted(uint32_t flags, const void* id);
        virtual void            onLastWeakRef(const void* id);

    private:
        friend class weakref_type;
        class weakref_impl;

        RefBase(const RefBase& o);
        RefBase& operator=(const RefBase& o);

        weakref_impl* const mRefs;
    };

    // ---------------------------------------------------------------------------

    template <class T>
    class LightRefBase
    {
    public:
        inline LightRefBase() : mCount(0) { }
        // 增加引用计数
        inline void incStrong(const void* id) const {
            android_atomic_inc(&mCount);
        }

        //// 减少引用计数
        inline void decStrong(const void* id) const {
            if (android_atomic_dec(&mCount) == 1) {
                //引用计数为 0 时，释放对象所占的内存
                delete static_cast<const T*>(this);
            }
        }
        //! DEBUGGING ONLY: Get current strong ref count.
        inline int32_t getStrongCount() const {
            return mCount;
        }

    protected:
        inline ~LightRefBase() { }

    private:
        mutable volatile int32_t mCount;
    };

    // ---------------------------------------------------------------------------

    template <typename T>
    class sp
    {
    public:
        typedef typename RefBase::weakref_type weakref_type;

        inline sp() : m_ptr(0) { }

        sp(T* other);
        sp(const sp<T>& other);
        template<typename U> sp(U* other);
        template<typename U> sp(const sp<U>& other);

        ~sp();

        // Assignment

        sp& operator = (T* other);
        sp& operator = (const sp<T>& other);

        template<typename U> sp& operator = (const sp<U>& other);
        template<typename U> sp& operator = (U* other);

        //! Special optimization for use by ProcessState (and nobody else).
        void force_set(T* other);

        // Reset

        void clear();

        // Accessors

        inline  T& operator* () const { return *m_ptr; }
        inline  T* operator-> () const { return m_ptr; }
        inline  T* get() const { return m_ptr; }

        // Operators

        COMPARE(== )
            COMPARE(!= )
            COMPARE(> )
            COMPARE(< )
            COMPARE(<= )
            COMPARE(>= )

    private:
        template<typename Y> friend class sp;
        template<typename Y> friend class wp;

        // Optimization for wp::promote().
        sp(T* p, weakref_type* refs);

        // 在构造函数中初始化，指向实际引用的对象。
        T* m_ptr;
    };



    // ---------------------------------------------------------------------------

    template <typename T>
    class wp
    {
    public:
        typedef typename RefBase::weakref_type weakref_type;

        inline wp() : m_ptr(0) { }

        wp(T* other);
        wp(const wp<T>& other);
        wp(const sp<T>& other);
        template<typename U> wp(U* other);
        template<typename U> wp(const sp<U>& other);
        template<typename U> wp(const wp<U>& other);

        ~wp();

        // Assignment

        wp& operator = (T* other);
        wp& operator = (const wp<T>& other);
        wp& operator = (const sp<T>& other);

        template<typename U> wp& operator = (U* other);
        template<typename U> wp& operator = (const wp<U>& other);
        template<typename U> wp& operator = (const sp<U>& other);

        void set_object_and_refs(T* other, weakref_type* refs);

        // promotion to sp

        sp<T> promote() const;

        // Reset

        void clear();

        // Accessors

        inline  weakref_type* get_refs() const { return m_refs; }

        inline  T* unsafe_get() const { return m_ptr; }

        // Operators

        COMPARE(== )
            COMPARE(!= )
            COMPARE(> )
            COMPARE(< )
            COMPARE(<= )
            COMPARE(>= )

    private:
        template<typename Y> friend class sp;
        template<typename Y> friend class wp;

        T* m_ptr;
        weakref_type* m_refs;
    };


#undef COMPARE

    // ---------------------------------------------------------------------------
    // No user serviceable parts below here.

    template<typename T>
    sp<T>::sp(T* other)
        : m_ptr(other)
    {
        if (other) other->incStrong(this);
    }

    template<typename T>
    sp<T>::sp(const sp<T>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) m_ptr->incStrong(this);
    }

    template<typename T> template<typename U>
    sp<T>::sp(U* other) : m_ptr(other)
    {
        if (other) other->incStrong(this);
    }

    template<typename T> template<typename U>
    sp<T>::sp(const sp<U>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) m_ptr->incStrong(this);
    }

    template<typename T>
    sp<T>::~sp()
    {
        if (m_ptr) m_ptr->decStrong(this);
    }

    template<typename T>
    sp<T>& sp<T>::operator = (const sp<T>& other) {
        T* otherPtr(other.m_ptr);
        if (otherPtr) otherPtr->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }

    template<typename T>
    sp<T>& sp<T>::operator = (T* other)
    {
        if (other) other->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }

    template<typename T> template<typename U>
    sp<T>& sp<T>::operator = (const sp<U>& other)
    {
        U* otherPtr(other.m_ptr);
        if (otherPtr) otherPtr->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = otherPtr;
        return *this;
    }

    template<typename T> template<typename U>
    sp<T>& sp<T>::operator = (U* other)
    {
        if (other) other->incStrong(this);
        if (m_ptr) m_ptr->decStrong(this);
        m_ptr = other;
        return *this;
    }

    template<typename T>
    void sp<T>::force_set(T* other)
    {
        other->forceIncStrong(this);
        m_ptr = other;
    }

    template<typename T>
    void sp<T>::clear()
    {
        if (m_ptr) {
            m_ptr->decStrong(this);
            m_ptr = 0;
        }
    }

    template<typename T>
    sp<T>::sp(T* p, weakref_type* refs)
        : m_ptr((p&& refs->attemptIncStrong(this)) ? p : 0)
    {
    }


    // ---------------------------------------------------------------------------

    template<typename T>
    wp<T>::wp(T* other)
        : m_ptr(other)
    {
        if (other) m_refs = other->createWeak(this);
    }

    template<typename T>
    wp<T>::wp(const wp<T>& other)
        : m_ptr(other.m_ptr), m_refs(other.m_refs)
    {
        if (m_ptr) m_refs->incWeak(this);
    }

    template<typename T>
    wp<T>::wp(const sp<T>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = m_ptr->createWeak(this);
        }
    }

    template<typename T> template<typename U>
    wp<T>::wp(U* other)
        : m_ptr(other)
    {
        if (other) m_refs = other->createWeak(this);
    }

    template<typename T> template<typename U>
    wp<T>::wp(const wp<U>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = other.m_refs;
            m_refs->incWeak(this);
        }
    }

    template<typename T> template<typename U>
    wp<T>::wp(const sp<U>& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_refs = m_ptr->createWeak(this);
        }
    }

    template<typename T>
    wp<T>::~wp()
    {
        if (m_ptr) m_refs->decWeak(this);
    }

    template<typename T>
    wp<T>& wp<T>::operator = (T* other)
    {
        weakref_type* newRefs =
            other ? other->createWeak(this) : 0;
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = newRefs;
        return *this;
    }

    template<typename T>
    wp<T>& wp<T>::operator = (const wp<T>& other)
    {
        weakref_type* otherRefs(other.m_refs);
        T* otherPtr(other.m_ptr);
        if (otherPtr) otherRefs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = otherRefs;
        return *this;
    }

    template<typename T>
    wp<T>& wp<T>::operator = (const sp<T>& other)
    {
        weakref_type* newRefs =
            other != NULL ? other->createWeak(this) : 0;
        T* otherPtr(other.m_ptr);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = newRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (U* other)
    {
        weakref_type* newRefs =
            other ? other->createWeak(this) : 0;
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = newRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (const wp<U>& other)
    {
        weakref_type* otherRefs(other.m_refs);
        U* otherPtr(other.m_ptr);
        if (otherPtr) otherRefs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = otherRefs;
        return *this;
    }

    template<typename T> template<typename U>
    wp<T>& wp<T>::operator = (const sp<U>& other)
    {
        weakref_type* newRefs =
            other != NULL ? other->createWeak(this) : 0;
        U* otherPtr(other.m_ptr);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = otherPtr;
        m_refs = newRefs;
        return *this;
    }

    template<typename T>
    void wp<T>::set_object_and_refs(T* other, weakref_type* refs)
    {
        if (other) refs->incWeak(this);
        if (m_ptr) m_refs->decWeak(this);
        m_ptr = other;
        m_refs = refs;
    }

    template<typename T>
    sp<T> wp<T>::promote() const
    {
        return sp<T>(m_ptr, m_refs);
    }

    template<typename T>
    void wp<T>::clear()
    {
        if (m_ptr) {
            m_refs->decWeak(this);
            m_ptr = 0;
        }
    }


}; // namespace android

// ---------------------------------------------------------------------------