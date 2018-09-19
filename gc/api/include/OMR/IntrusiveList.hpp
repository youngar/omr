
#if !defined(OMR_INTRUSIVELIST_HPP_)
#define OMR_INTRUSIVELIST_HPP_

#include <type_traits>
#include <utility>

namespace OMR
{

template<typename T>
class IntrusiveList;

template<typename T>
class IntrusiveListNode;

template<typename T>
class IntrusiveListIterator;

/// Functor for obtaining the IntrusiveListNode from an element in the list.
/// The default implementation will call element.getIntrusiveListNode().
/// Can be specialized if custom accessor is needed.
template<typename T>
struct GetIntrusiveListNode
{
	constexpr IntrusiveListNode<T>& operator()(T&& element) const noexcept
	{
		return element.node();
	}
};

/// Specialization for T* that defers to GetIntrusiveListNode<T>::operator()
template<typename T>
struct GetIntrusiveListNode<T*>
{
	constexpr IntrusiveListNode<T>& operator()(T* element) const noexcept
	{
		return GetIntrusiveListNode<T>()(*element);
	}
};

/// Stores the next and previous pointers for elements in an IntrusiveList.
/// Each element in an intrusive list must contain an IntrusiveListNode.
/// Elements must make their node accessible through GetIntrusiveListNode::operator()(T&).
template<typename T>
class IntrusiveListNode
{
public:
	constexpr IntrusiveListNode() : nextElement(nullptr), prevElement(nullptr) {}

private:
	friend class IntrusiveList<T>;
	friend class IntrusiveListIterator<T>;

	constexpr IntrusiveListNode<T>& nextNode() const noexcept
	{
		return GetIntrusiveListNode<T*>()(nextElement);
	}

	constexpr IntrusiveListNode<T>& prevNode() const noexcept
	{
		return GetIntrusiveListNode<T*>()(prevElement);
	}

	void assign(T* next, T* prev) noexcept
	{
		nextElement = next;
		prevElement = prev;
	}

	// deactivate the node, and clear the next/prev pointers.
	void deactivate() noexcept
	{
		nextElement = nullptr;
		prevElement = nullptr;
	}

	/// true if this node is attached to a list.
	constexpr bool active() const noexcept { return nextElement != nullptr; }

	T* nextElement;
	T* prevElement;
};

template<typename T>
class IntrusiveListIterator
{
public:
	IntrusiveListIterator(const IntrusiveListIterator<T>&) = default;

	constexpr T& operator*() const noexcept { return *_currentElement; }

	constexpr T* operator->() const noexcept { return _currentElement; }

	IntrusiveListIterator<T>& operator++() noexcept
	{
		_currentElement = currentNode().nextElement;
		if (_currentElement == _rootElement) {
			// we are back at the root, clear _current.
			_currentElement = nullptr;
		}
		return *this;
	}

	constexpr bool operator==(T* element) const noexcept { return _currentElement == element; }

	constexpr bool operator==(const IntrusiveListIterator<T>& rhs) const noexcept
	{
		return _currentElement == rhs._currentElement;
	}

	constexpr bool operator!=(T* element) const noexcept { return _currentElement != element; }

	constexpr bool operator!=(const IntrusiveListIterator<T>& rhs) const noexcept
	{
		return _currentElement != rhs._currentElement;
	}

	IntrusiveListIterator<T>& operator=(const IntrusiveListIterator<T>& rhs) noexcept
	{
		_rootElement = rhs._rootElement;
		_currentElement = rhs._currentElement;
		return *this;
	}

private:
	friend class IntrusiveList<T>;

	constexpr IntrusiveListIterator(T* root) : _rootElement(root), _currentElement(root) {}

	constexpr IntrusiveListNode<T>& currentNode() const noexcept
	{
		return GetIntrusiveListNode<T*>()(_currentElement);
	}

	T* _rootElement;
	T* _currentElement;
};

/// A linked list, where the next and previous pointers are stored in a node, which is embedded in the element type T.
///
/// To use an intrusive list, the element type T must store an IntrusiveListNode<T>.
///
template<typename T>
class IntrusiveList
{
public:
	constexpr IntrusiveList() noexcept : _root(nullptr) {}

	/// Add element to the list.
	void add(T* element) noexcept
	{
		if (empty()) {
			_root = element;
			getNode(element).assign(element, element);
		} else {
			auto& rootnode = getNode(_root);
			T* lastElement = rootnode.prevElement;
			auto& lastnode = getNode(lastElement);
			lastnode.nextElement = element;
			rootnode.prevElement = element;
			auto& newNode = getNode(element);
			newNode.prevElement = lastElement;
			newNode.nextElement = _root;
		}
	}

	/// Remove element from the list. Removing an element invalidates any iterators.
	void remove(T* element) noexcept
	{
		IntrusiveListNode<T>& node = getNode(element);
		if (isRoot(element)) {
			if (isLast(element)) {
				// we are removing the only element, and have to clear the root.
				_root = nullptr;
			} else {
				_root = node.nextElement;
				node.prevNode().nextElement = node.nextElement;
				node.nextNode().prevElement = node.prevElement;
			}
		} else {
			node.prevNode().nextElement = node.nextElement;
			node.nextNode().prevElement = node.prevElement;
		}
		node.deactivate();
	}

	constexpr IntrusiveListIterator<T> begin() const noexcept
	{
		return IntrusiveListIterator<T>(_root);
	}

	constexpr IntrusiveListIterator<T> end() const noexcept
	{
		return IntrusiveListIterator<T>(nullptr);
	}

	constexpr bool empty() const noexcept { return _root == nullptr; }

private:
	IntrusiveListNode<T>& getNode(T* element) { return GetIntrusiveListNode<T*>()(element); }

	// true if element is in the final position of the list.
	bool isLast(T* element) { return element == getNode(_root).prevElement; }

	bool isRoot(T* element) { return element == _root; }

	IntrusiveListNode<T>& rootNode() const noexcept { return getNode(_root); }

	T* _root;
};

} // namespace OMR

#endif // OMR_INTRUSIVELIST_HPP_
