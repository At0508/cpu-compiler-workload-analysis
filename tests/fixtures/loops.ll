define void @fixed_loop(ptr %data) {
entry:
  br label %loop

loop:
  %index = phi i64 [ 0, %entry ], [ %next, %loop ]
  %address = getelementptr inbounds i32, ptr %data, i64 %index
  %value = load i32, ptr %address, align 4
  %updated = add i32 %value, 1
  store i32 %updated, ptr %address, align 4
  %next = add nuw nsw i64 %index, 1
  %continue = icmp ult i64 %next, 16
  br i1 %continue, label %loop, label %exit

exit:
  ret void
}

define void @dynamic_loop(ptr %data, i64 %count) {
entry:
  br label %loop

loop:
  %index = phi i64 [ 0, %entry ], [ %next, %loop ]
  %address = getelementptr inbounds i32, ptr %data, i64 %index
  %value = load i32, ptr %address, align 4
  %next = add nuw nsw i64 %index, 1
  %continue = icmp ult i64 %next, %count
  br i1 %continue, label %loop, label %exit

exit:
  ret void
}
