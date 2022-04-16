#include <iostream>
using std::cout;
using std::endl;

int cnt = 0;

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

    template<typename ...Ty>    //  不定个数参数
    void construct(T *p,Ty&&... val)    //  引用折叠
    {
        new (p) T(std::forward<Ty>(val)...);    //  ...不定参数+完美转发 传入相应构造函数
    }
    /*
    template<typename Ty>
    void construct(T* p, Ty&& val)
    {
        new (p) T(std::forward<Ty>(val));
    }
    */
    /*
    void construct(T* p, const T& val)   //  负责对象构造(在已有的内存上)
    {
        new (p) T(val);     //  定位new  在p指向的内存上构造(拷贝构造)
    }
    void construct(T* p, T&& rval)   //  参数val匹配右值
    {
        //  在p处定位new 调用T的拷贝构造函数。
        //  rval这个右值引用变量本身为左值；但是他所引用的那个变量val是个右值。因此我们要通过mover(val)来实现类型的转换
        //  move实现的是语法层面的转换，告诉编译器rval是个右值，让他匹配右值拷贝构造函数。并没有对rval本身内存做出什么更改的事情
        // 只是改变了语法层面的类型，为了让编译器识别出他（所引用的）是个右值
        //  因为val为右值，因此调用拷贝构造函数时，匹配的就是T(&&)
        new (p) T(std::move(rval)); 
    }
    */

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

    vector<T,Alloc>& operator=(const vector<T,Alloc>& rhs)  //  拷贝赋值
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


    //  emplace_back实现 引用折叠 + forward万能转发 + 不定模板参数
    //  emplace实现原地构造，避免临时量的产生：其实就是把原本要构造临时量对象的参数直接传进来，直接在vector容器的内存上定位new，
    //  即调用相应构造函数，而不是先构造临时量对象、传入临时量对象，再在vector容器的内存上调用拷贝构造。从而避免一个临时对象的拷贝和析构
    template<typename ...Ty>    //  不定模板参数 ...代表有很多模板参数
    void emplace_back(Ty&&... args)
    {
        this->_allocator.construct(_last++,std::forward<Ty>(args)...);  //  ...代表不定个数的参数
    }


    //  采用引用折叠 + forward完美转发
    //  引用折叠：推导出val到底是右值引用还是左值引用。使得函数可以同时接收左值引用和右值引用。
    //  forward：传参时保留val他（所引用的）到底是一个左值还是右值的信息，即保留引用的类型。
    //  避免左值引用和右值引用变量本身都是左值而失去左右值信息，那样传参就只能传给左值。
    template<typename Ty>
    void push_back(Ty&& val)        //  CMyString&& + && -》 CMyString&& ; CMyString& + && -> CMyString&
    {
        if (full())
            expand();
        _allocator.construct(_last++, std::forward<Ty>(val));
    }
    //  push_back需要写两个 但是核心只有参数列表不同，右值还需要通过move来表示。不同也只是为了向下调用时调用到与左/友值匹配的函数。
    /*
    void push_back(const T& val)
    {
        if (full())
            expand();
        //        *_last++ = val;     //  元素的operator=
        _allocator.construct(_last++, val);
    }

    //  push_back(&&) -> construct(&&) -> new T(&&)（也就是调用 类型的右值拷贝构造，如CMyString(&&))
    //  传入的实参为右值时（如CMyString("aa");会匹配到这里)
    //  push_back了之后，容器里装的对象就和push_back左值一样。后续对于容器的操作也一样.
    void push_back(T&& val)     //  push_back右值
    {
        if (full())
            expand();
        //  因为右值变量本身是左值，因此要move一下，让编译器识别他是右值。
        _allocator.construct(_last++, std::move(val));  
    }
    */

    //  需要析构对象，并且要把析构对象和释放内存的操作分离开
    void pop_back()
    {
        if(empty())
            return ;
        verify(_last-1,_last);  //  检查删除元素后，有哪些迭代器失效    _last-1：最后一个元素。有对应迭代器 _last：后继位置，有对应迭代器
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
        friend class vector<T,Alloc> ;
        //  产生迭代器对象 自动将自己加入迭代器链表
        iterator(vector<T,Alloc> *pvec , T *p = nullptr)
            :_pVec(pvec),_ptr(p)
        {
            cout<<cnt++<<endl;
            //  链表头插法                       节点内容    节点指向
            Iterator_Base *ib = new Iterator_Base(this,_pVec->_head._next);
            _pVec->_head._next = ib;        //  指针ib是局部变量，会被销毁无所谓。因为ib指向的内存是new出来的啊哈哈哈。不会被销毁的。
        }
        bool operator!=(const iterator& iter)
        {
            //  _pVec == nullptr 代表迭代器失效
            //  首先判断是否是同一容器的迭代器（比较迭代器指向容器的地址是否相同）
            if(_pVec == nullptr || _pVec != iter._pVec)
            {
                throw "iterator incompatable!";
            }
            return _ptr!=iter._ptr;
        }
        void operator++()
        {
            //  _pVec == nullptr 迭代器失效
            if(_pVec==nullptr)
            {
                throw "iterator inValid";
            }
            _ptr++;
        }
        T& operator*()
        {
            //  检查迭代器失效
            if(_pVec==nullptr)
            {
                throw "iterator inValid";
            }
            return *_ptr;
        }
        const T& operator*() const{return *_ptr;}
        ~iterator()
        {
            cout<<"hh"<<endl;
        }
    private:
        //  指向元素
        T *_ptr;
        //  指明是哪个容器的迭代器 T和Alloc都是已知的（外层vector就确定了） 因为同一迭代器的比较才有效
        vector<T,Alloc> *_pVec;     //  迭代器是否有效标志
    };

    //  容器需要提供begin end方法
    iterator begin(){return iterator(this , _first);}
    iterator end(){return iterator(this , _last);}

    iterator insert(iterator it,const T &val)
    {
        verify(it._ptr-1,_last);    //  [it._ptr , last]的迭代器 如果存在，都要失效
        T *p = _last;   //  p是指针 不是迭代器
        while(p > it._ptr)
        {
            _allocator.construct(p,*(p-1));
            _allocator.destroy(p-1);
            --p;
        }
        _allocator.construct(p,val);    //  插入
        ++_last;
        return iterator(this,p);
    }

    iterator erase(iterator iter)
    {
        verify(iter._ptr-1,_last);
        T *p = iter._ptr;
        while(p < _last-1)
        {
            _allocator.destroy(p);
            _allocator.construct(p,*(p+1));
            ++p;
        }
        _allocator.destroy(p);
        _last--;
        return iterator(this,iter._ptr);
    }


private:
    T * _first; //  指向数组起始位置
    T * _last;  //  指向数组中有效元素的后继位置
    T * _end;   //  指向数组空间的后继位置
    Alloc _allocator;   // 空间配置器对象

    //  为应对迭代器失效 增加代码

    //  用一个链表来维护每个迭代器之间的顺序（可以当成是一个用来维护（装载）迭代器的容器
    //  迭代器节点 _cur指向迭代器 _next指向下一个iterator_base节点
    struct Iterator_Base
    {
        Iterator_Base(iterator *c= nullptr,Iterator_Base *ne = nullptr)
            :_cur(c),_next(ne){}
        iterator *_cur;
        Iterator_Base *_next;
    };
    Iterator_Base _head;    //  一个容器只有一个迭代器头节点

    //  检查
    //  将(first,last]之间的元素，如果存在对应的迭代器，那么都要置为失效。并将维护他们的节点从链表中移除。
    //  遍历链表
    void verify(T* first , T* last)
    {
        Iterator_Base *pre = &this->_head;   //  头节点
        Iterator_Base *it = this->_head._next;   //  第一个有效节点
        while(it!=nullptr)
        {   //  it 节点 _cur 迭代器 _ptr 指向的元素
            if(it->_cur->_ptr > first && it->_cur->_ptr <= last)
            {
                //  标记迭代器失效 ：iterator持有的_pVec = nullptr
                it->_cur->_pVec = nullptr;
                pre->_next = it->_next;
                delete it;          //  delete掉用于维护迭代器的节点
                it = pre->_next;
            }
            else
            {
                pre = it;
                it = it->_next;
            }
        }
    }

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



//  测试代码

#if 1
int main()
{
    vector<int> vec;
    for(int i=0;i<3;++i) vec.push_back(rand()%20);
//    vector<int>::iterator it(Allocator<int>);
//    it = vec.begin(); 压根没有赋值函数
//    int i = 0;
//    vector<int>::iterator iter1 = vec.end();    //  end() 产生迭代器 迭代器加入链表
//    cout<<"!"<<endl;
//    vec.pop_back();
//    cout<<"?"<<endl;
//    vector<int>::iterator iter2 = vec.end();
//    cout<< (iter1!=iter2) <<endl;

//  每次调用end() 都会新产生一个迭代器（虽然他们要指向的位置是一致的，但是还是会产生） 也就是说 迭代器链表中会有很多指向位置相同的迭代器节点(*_cur)
//    for(vector<int>::iterator iter = vec.begin();iter!=vec.end();++iter)
//    {
//        ++i;
//    }
//    cout<<"-----------------------------"<<endl;
//    cout<<endl;
}
#endif


# if 0
int main() {
    vector<int> vec(40);
    for (int i = 0; i < 20; ++i) {
        vec.push_back(rand() % 100 + 1);
    }

    auto it = vec.begin();
    while (it != vec.end()) {
        if (*it % 2 == 0) {
            // 迭代器失效的问题，第一次调用erase以后，迭代器it就失效了
            it = vec.erase(it); // insert(it, val)   erase(it)
        } else {
            ++it;
        }
    }

    for (int v: vec) {
        cout << v << " ";
    }
    cout << endl;
}

#endif

# if 0

int main() {
    vector<int> v;
    for(int i=0;i<3;++i) v.push_back(rand()%20);
    for(int x:v) cout<<x<<" ";
    cout<<endl;

    auto it = v.begin();
    v.insert(it,*it-1);     // insert:当前位置增加新元素，原元素依次向后移动
    ++it;

    for(int x:v) cout<<x<<" ";
    cout<<endl;
    std::cout << "Hello, World!" << std::endl;

    return 0;
}
#endif

