#include <iostream>
using std::cout;
using std::endl;


/*
 * 容器的空间配置器 allocator
 * 内存的开辟释放,对象的构造析构,都是通过空间配置器完成.而不是直接通过new delete
 * template<class _Ty
 *      class _Alloc = allocator<_Ty>>
 *      class vector
 */

//  定义容器的空间配置器
template<typename T>
struct Allocator
{
    T *allocate(int size)   //  负责内存开辟
    {
        return (T*)malloc(sizeof(T)*size);
    }

    T *deallocate(void *p)  //  负责内存释放
    {
        free(p);
    }

    void construct(T *p,const T& val)   //  负责对象构造(在已有的内存上)
    {
        new (p) T(val);     //  定位new  在p指向的内存上构造(拷贝构造)
    }

    void destroy(T *p)      //  负责对象析构
    {
        p->~T();            //  ~T()
    }

};

//  容器底层内存开辟,内存释放,对象构造,对象析构,都通过allocator空间配置器来实现
                        //  类型Alloc 的类型默认是 空间配置器Allocator<T>  模板名称 + 类型参数 = 类名称
template<typename T , typename Alloc = Allocator<T>>
class vector
{
public:                        // 默认形参     构造函数:Allocator<T>()
    vector<T,Alloc>(int sz = 10,const Alloc& alloc = Allocator<T>())
            :_allocator(alloc)
    {
        //  需要把内存开辟和对象构造分开处理    不然在创建vector的时候，不但开辟内存，还构造了所有元素.正常逻辑应当是我们加入元素.
//        _first = new T[sz];
        _first = _allocator.allocate(sz);   //  开辟内存
        _last = _first;
        _end = _first + sz;
    }

    ~vector() {
//        delete [] _first;
        //  析构有效对象
        for (T *p = _first; p != _last; ++p)
        {
            _allocator.destroy(p);  //  析构p对象
        }
        //  ,并释放所有空间
        _allocator.deallocate(_first);
        _first = _last = _end = nullptr;
    }

    vector(const vector<T,Alloc>& rhs)  //  拷贝构造
    {
        int sz = rhs._end - rhs._first;     //  容器大小
//        _first = new T[sz];
        _first = _allocator.allocate(sz);
        _end = _first + sz;
        int len = rhs._last - rhs._first;   //  有效元素长度
        for(int i=0;i<len;++i)
        {
            _allocator.construct(_first[i],rhs.first[i]);
        }
        _last = _first + len;
    }

    vector<T>& operator=(const vector<T,Alloc>& rhs)  //  拷贝赋值
    {
        if (this == &rhs)      //  与拷贝构造区别：避免自赋值
            return *this;

//        delete [] _first;   //   析构所有对象 销毁原内存
        //  析构有效对象 释放所有内存
        for (T *p = _first; p != _last; ++p)
        {
            _allocator.destroy(p);
        }
        _allocator.deallocate(_first);

        int sz = rhs._end - rhs._first;
//        _first = new T[sz];
        _first = _allocator.allocate(sz);   // 分配内存
        int len = rhs._last - rhs._first;
        for(int i=0;i<len;++i)
        {
            _allocator.construct(_first+i,rhs._first[i]) ;  //  构造有效对象
        }
        _last = _first + len;
        _end = _first + sz;
        return *this;
    }

    void push_back(const T& val)
    {
        if(full())
            expand();
//        *_last++ = val;     //  元素的operator=
        _allocator.construct(_last++,val);
    }

    //  需要析构对象，并且要把析构对象和释放内存的操作分离开
    void pop_back()
    {
        if(empty())
            return ;
//        --_last;        //  只移动指针,并没有析构元素,也没有释放元素管理的内存。
        _allocator.destroy(--_last);    //  不仅移动指针 还需要析构元素s
    }

    T back() const
    {
        if(empty())
            throw "vector is empty";
        return *(_last-1);
    }

    bool empty() const{return _first==_last;}
    bool full() const{return _last==_end;}
    int size() const{return _last - _first;}
    T& operator[](int index){
        if(index<0 || index>=size())
            throw "OutOfRange";
        return _first[index];
    }
    const T& operator[](int index) const{
        if(index<0 || index>=size())
            throw "OutOfRange";
        return _first[index];
    }
    //  实现迭代器
    //  迭代器一般实现成容器的嵌套类型
    class iterator  //  迭代器就是包装了遍历方法(++)的一个指针
    {
    public:
        iterator(T *p = nullptr)
            :_ptr(p){}
        bool operator!=(const iterator& iter)
        {
            return _ptr!=iter._ptr;
        }
        iterator& operator++()
        {
            _ptr++;
            return *this;
        }
        T& operator*(){ return *_ptr; }
        const T& operator*() const{return *_ptr;}
    private:
        T *_ptr;
    };
    //  需要给容器提供begin end方法
    iterator begin(){return iterator(_first);}
    iterator end(){return iterator(_last);}

private:
    T * _first; //  指向数组起始位置
    T * _last;  //  指向数组中有效元素的后继位置
    T * _end;   //  指向数组空间的后继位置
    Alloc _allocator;   // 空间配置器对象
    //  容器的二倍扩容操作
    void expand()
    {
        int sz = _end - _first;
        int len = _last - _first;
//        T *p_tmp = new T[sz*2];
        //  开辟内存 构造对象
        T *p_tmp = _allocator.allocate(sz*2);
        for(int i=0;i<len;++i)
        {
//            p_tmp[i] = _first[i];
            _allocator.construct(p_tmp+i,_first[i]);
        }

        //  析构对象 释放内存
        for(T *p = _first;p!=_last;++p)
        {
            _allocator.destroy(p);
        }
//        delete [] _first;   //  释放原先内存 析构所有对象
        _allocator.deallocate(_first);

        _first = p_tmp;
        _last = _first + len;
        _end = _first + 2*sz;
        return ;
    }
};


int main()
{
    vector<int> vec;
    for(int i=0;i<20;++i) vec.push_back(rand()%20);
    for(vector<int>::iterator iter = vec.begin();iter!=vec.end();++iter)
    {
        cout<<*iter<<" ";
    }
    cout<<endl;

    for(int x:vec)
    {
        cout<<x<<' ';
    }
    cout<<endl;
}


# if 0
class Test
{
public:
    Test(){cout<<"Test()"<<endl;}
    ~Test(){cout<<"~Test()"<<endl;}
    Test(const Test& t)
    {
        cout<<"Test(const Test& t)"<<endl;
    }
    Test& operator=(const Test& t)
    {
        cout<<"Test& operator="<<endl;
        return *this;
    }
};

int main()
{
    Test t1,t2,t3;
    cout<<"----------------"<<endl;
    vector<Test> v;         //  只开辟了空间 没有构造对象
    v.push_back(t1);    //  Test(const Test& )
    v.push_back(t2);
    v.push_back(t3);
    cout<<"----------------"<<endl;
    v.pop_back();           //  调用析构 不是释放内存
    cout<<"----------------"<<endl;
}
#endif