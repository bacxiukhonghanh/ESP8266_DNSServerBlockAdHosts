#ifndef DomainsList_h
#define DomainsList_h

class DomainsList
{
    // Access specifier
    public:
    // Data Members
    /*
     * Please note that the number of elements in the "domains" array in the declaration below
     * has to be the same with the number of domains that are assigned inside the constructor.
     * If the number of elements in the "domains" array is less than the number of domains
     * assigned inside the constructor, the sketch might not run due to the out-of-bound index
     * in the constructor.
     * If the number of elements in the "domains" array is more than the number of domains
     * assigned inside the constructor, the code in MyDNSServer.cpp might throw an error
     * because not all the elements in the "domains" array are initialized.
     * On a NodeMCU board that I tested, I can put at most 500 domains into this array
     * due to memory limitations.
    */
    String domains[2];
 
    // Member Functions()
    DomainsList() {
      domains[0] = "domain-to-block-here.com";
      domains[1] = "more-domain-to-block.net";
    };
};

#endif
