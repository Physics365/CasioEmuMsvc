#pragma once
#include <mutex>
template <class T>
class ConcurrentObject {
protected:
	mutable std::mutex mtx;
	T* storage;

public:
	ConcurrentObject() {
		storage = new T();
	}
	ConcurrentObject(const T& obj) {
		storage = new T(obj);
	}
	ConcurrentObject(T&& obj) {
		storage = &obj;
	}
	~ConcurrentObject() {
		delete storage;
	}
	class ObjectRef {
	public:
		ConcurrentObject<T>& obj;
		bool own_lock = false;
		ObjectRef(ConcurrentObject<T>& obj) : obj(obj) {
			own_lock = obj.mtx.try_lock();
		}
		~ObjectRef() {
			if (own_lock)
				obj.mtx.unlock();
		}
		T* operator->() {
			return obj.storage;
		}
		T& operator*() {
			return *obj.storage;
		}
	};
	class ConstObjectRef {
	public:
		const ConcurrentObject<T>& obj;
		bool own_lock = false;
		ConstObjectRef(const ConcurrentObject<T>& obj) : obj(obj) {
			own_lock = obj.mtx.try_lock();
		}
		~ConstObjectRef() {
			if (own_lock)
				obj.mtx.unlock();
		}
		const T* operator->() {
			return obj.storage;
		}
		const T& operator*() {
			return *obj.storage;
		}
	};
	auto get() {
		return ObjectRef{*this};
	}
	auto get_const() const {
		return ConstObjectRef{*this};
	}
};