/**
 * @file rpc.hpp
 *
 * Basic idea behind this Remote Procedure Call abstraction is,
 * to have a bidirectional, binary stream over which synchronous remote function calls
 * are serialized and performed. So, all functions on the remote side have to be
 * none blocking.
 *
 * The usuall aproach for a basic RPC implementation is to serialize a function call,
 * by sending an opcode (that identified the function to be called on the remote side),
 * followed by the serialized function arguments of the call and then wait for
 * the serialized function return value.
 *
 * To be able to call remote prodcedures in both directions, there is a need to distiquish between
 * the opcode of a function call (this is the one direction) and the function return value in the
 * oposite direction. This can be solved by using a binary direction marker in front of the
 * opcode/ function return value.
 *
 * As the set of functions to be called needs to be known by both sides, the opcode can be
 * derived from an order of the functions (which then have to be equal for both).
 *
 * The set of function arguments and the function return type can be deduced by prototypes of
 * the function to be called.
 */
