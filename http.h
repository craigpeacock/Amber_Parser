
struct buffer {
	char *data;
	size_t pos;
};

int http_json_request(struct buffer *out_buf, char *postcode);
