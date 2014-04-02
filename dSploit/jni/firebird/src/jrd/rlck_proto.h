/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		rlck_proto.h
 *	DESCRIPTION:	Prototype header file for rlck.cpp
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_RLCK_PROTO_H
#define JRD_RLCK_PROTO_H

namespace Jrd {
	class Lock;
	class jrd_rel;
	class jrd_tra;
	struct record_param;
	class Attachment;
	class thread_db;
}

Jrd::Lock* RLCK_reserve_relation(Jrd::thread_db*, Jrd::jrd_tra*, Jrd::jrd_rel*, bool);
Jrd::Lock* RLCK_transaction_relation_lock(Jrd::thread_db*, Jrd::jrd_tra*, Jrd::jrd_rel*);

#endif // JRD_RLCK_PROTO_H

