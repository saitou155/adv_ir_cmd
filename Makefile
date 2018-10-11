CC      = cc
OBJS    = adv_ir_cmd.o

adv_ir_cmd: $(OBJS)
	$(CC) -Wall -g -o $@ $(OBJS) -lusb-1.0

.c.o:
	$(CC) -c -g $<

clean:
	@rm -f $(OBJS)
	@rm -f adv_ir_cmd
