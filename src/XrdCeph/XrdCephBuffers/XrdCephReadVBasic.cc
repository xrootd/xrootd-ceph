
#include "XrdCephReadVBasic.hh"
#include "BufferUtils.hh"

using namespace XrdCephBuffer;

std::vector<ExtentHolder> XrdCephReadVBasic::convert(const ExtentHolder &extentsHolderInput) const
{
    std::vector<ExtentHolder> outputs;

    const ExtentContainer &extentsIn = extentsHolderInput.extents();

    ExtentContainer::const_iterator it = extentsIn.begin();
    while (it != extentsIn.end())
    {
        ExtentHolder tmp;
        int counter(0);
        while (it != extentsIn.end()) {
            tmp.push_back(*it); // just put it into an extent
            ++it;
            ++counter;
            if (counter > 10 ) break;
        }
        // while (it != extentsIn.end())
        // {
        //     //std::clog << "XrdCephReadVBasic: Inner: " << it->begin() << " " << it->len() << std::endl;
        //     if (!tmp.size())
        //     {
        //         tmp.push_back(*it);
        //     }
        //     else if (it->end() - tmp.begin() < (ssize_t)m_minSize)
        //     {
        //         tmp.push_back(*it);
        //     }
        //     else if (((tmp.bytesContained() + it->len()) / (tmp.len() + it->len())) > 0.6)
        //     {
        //         tmp.push_back(*it);
        //     }
        //     else if (it->end() - tmp.begin() >= (ssize_t)m_maxSize)
        //     {
        //         break; // don't make too big
        //     }
        //     else
        //     {
        //         break; // didn't fullful logic to include, so start a new extent in next loop
        //     }
        //     ++it;
        // }
        //BUFLOG("XrdCephReadVBasic: Done Inner: " << tmp.size());
        outputs.push_back(tmp);
    }
    BUFLOG("XrdCephReadVBasic: In size: " << extentsHolderInput.size() << " " 
            << extentsHolderInput.extents().size() << " " << outputs.size() );

    return outputs;
} // convert
