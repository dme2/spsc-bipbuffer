/* *
 * single consumer, single producer bip buffer is defined here
 *
 * What is a bip(artite) buffer?
 *   the concept is similiar to a circular buffer - data is to fill the buffer in a (semi) continous fashion. This results in
 *   synchronization difficulties with regards to reads/writes. In order to overcome these difficulties we maintain two pointers
 *   on our fixed size buffer. This typically works well for certain applications - but most of the time we're not looking to
 *   pass two pointers around in our program, we want a solid continuous block of data. This is where the bipbuffer comes in
 *   it allows us to maintain a continous block of memory. The bip part comes from it being bipartate (i.e. composed of two
 *   regions, typically denoted A and B).
 *
 *   The main idea of the bipbuffer is as follows:
 *      space is reserved (this denotes the region as region A or region B) (reserve)
 *      data is inserted in the buffer such that is remains continues (commit)
 *      data can the be read and (or) decommited from the buffer, freeing up space in that region
 *
 * Why SCSP?
 *   because reserving, storing, and reading from a buffer are all potentially expensive and error prone operations, especially
 *   in something like audio programming where reads and writes happen constantly and missed data can result in bad souding audio
 *   - we want 1. speed and 2. consistancy. We can get speed from splitting theese jobs between threads. One produces and the
 *   other consumes. And we can get consistancy from ensuring that we're not sending bad data (if we don't get the data in time
 *   should we send silence??) and we can potentially do that by blocking until we have at least the correct amount of data.
 *
 *
 * */

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
