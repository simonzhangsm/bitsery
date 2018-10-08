
/**
 * Supports serialization of objects and polymorphic objects.
 * Example of non polymorphic serialization:
 * ~~~
 * */

#include <bitsery/details/archive.h>

#include <iostream>

  class point
  {
  public:
      point() = default;
      point(int x, int y) noexcept :
            m_x(x),
            m_y(y)
        {
        }

      friend bitsery::serializer::access;
      template <typename Archive, typename Self>
      static void serialize(Archive & archive, Self & self)
      {
          archive(self.m_x, self.m_y);
      }

      int get_x() const noexcept
      {
            return m_x;
        }

        int get_y() const noexcept
        {
            return m_y;
        }

  private:
      int m_x = 0;
      int m_y = 0;
  };

  static void foo()
  {
      std::vector<unsigned char> data;
      bitsery::serializer::memory_input_archive in(data);
      bitsery::serializer::memory_output_archive out(data);

      out(point(1337, 1338));

      point my_point;
      in(my_point);

      std::cout << my_point.get_x() << ' ' << my_point.get_y() << '\n';
  }
 /**
 * ~~~
 *
 * Example of polymorphic serialization:
 * ~~~
 **/
  class person : public bitsery::serializer::polymorphic
  {
  public:
      person() = default;
      explicit person(std::string name) noexcept :
            m_name(std::move(name))
        {
        }

      friend bitsery::serializer::access;
      template <typename Archive, typename Self>
      static void serialize(Archive & archive, Self & self)
      {
            archive(self.m_name);
        }

        const std::string & get_name() const noexcept
        {
            return m_name;
        }

      virtual void print() const
      {
          std::cout << "person: " << m_name;
      }

  private:
      std::string m_name;
  };

  class student : public person
  {
  public:
      student() = default;
      student(std::string name, std::string university) noexcept :
          person(std::move(name)),
          m_university(std::move(university))
      {
      }

      friend bitsery::serializer::access;
      template <typename Archive, typename Self>
      static void serialize(Archive & archive, Self & self)
      {
          person::serialize(archive, self);
            archive(self.m_university);
        }

      virtual void print() const
      {
          std::cout << "student: " << person::get_name() << ' ' << m_university << '\n';
      }

  private:
      std::string m_university;
  };

  namespace
  {
  bitsery::serializer::register_types<
     bitsery::serializer::make_type<person, bitsery::serializer::make_id("v1::person")>,
     bitsery::serializer::make_type<student, bitsery::serializer::make_id("v1::student")>
  > _;
  } // <anynymous namespace>

  static void foo()
  {
      std::vector<unsigned char> data;
      bitsery::serializer::memory_input_archive in(data);
      bitsery::serializer::memory_output_archive out(data);

      std::unique_ptr<person> my_person = std::make_unique<student>("1337", "1337University");
      out(my_person);

      my_person = nullptr;
      in(my_person);

      my_person->print();
  }

  static void bar()
  {
      std::vector<unsigned char> data;
      bitsery::serializer::memory_input_archive in(data);
      bitsery::serializer::memory_output_archive out(data);

      out(bitsery::serializer::as_polymorphic(student("1337", "1337University")));

      std::unique_ptr<person> my_person;
      in(my_person);

      my_person->print();
  }




